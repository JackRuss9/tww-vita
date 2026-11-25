#include "gpu.h"
#include "pc/pc_harness.h"

#include <cstdlib>
#include <exception>

#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <aurora/fatal.hpp>
#include <aurora/legacy_gx.h>
#include <aurora/vita_host_alloc.hpp>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>

extern "C" unsigned int aurora_vita_presented_frames();
extern "C" void tww_vita_gxm_wait_totals(uint64_t* staging, uint64_t* finish);
extern "C" void tww_vita_perf_totals(unsigned long long *out, unsigned int count);
extern "C" unsigned long long aurora_vita_fifo_drain_us();
extern "C" int aurora_vita_gx_thread();
extern "C" void aurora_vita_gx_thread_totals(unsigned long long* workerUs, unsigned long long* waitUs,
                                             unsigned long long* publishes);
extern "C" void tww_vita_settings_frame();
extern "C" void tww_vita_autores_note(uint64_t now_us, uint64_t wait_us);

namespace {
SceUID boot_log = -1;
bool first_present_logged;
bool first_frame_admitted;
bool first_frame_submitted;
uint64_t frame_begin_us, total_admission_us, total_game_us, total_submit_us;
uint64_t total_events_us;

void aurora_fatal(const char *message)
{
    probe_log(boot_log, "aurora fatal: %s\n", message);
    pc_exit(1);
}

void aurora_gx_panic(const char *entry)
{
    probe_log(boot_log, "aurora GX panic: %s\n", entry);
    if (boot_log >= 0)
        sceIoSyncByFd(boot_log, 0);
    // Aurora still aborts so the Vita records a crash dump.
}

[[noreturn]] void report_terminate() noexcept
{
    const auto exception = std::current_exception();
    if (exception) {
        try {
            std::rethrow_exception(exception);
        } catch (const std::exception &error) {
            probe_log(boot_log, "vita-unhandled-exception: %s\n", error.what());
        } catch (...) {
            probe_log(boot_log, "vita-unhandled-exception: unknown type\n");
        }
    }
    pc_milestone("vita-terminate");
    std::abort();
}

void check_presentation()
{
    if (!first_present_logged && aurora_vita_presented_frames() != 0) {
        first_present_logged = true;
        pc_milestone("vita-first-frame-presented");
    }
}

void aurora_log(AuroraLogLevel level, const char *module, const char *message, unsigned int size)
{
    probe_log(boot_log, "aurora: %s: %.*s\n", module, static_cast<int>(size), message);
    if (level >= LOG_ERROR && boot_log >= 0)
        sceIoSyncByFd(boot_log, 0);
}
}

extern "C" int tww_vita_gxm_init(SceUID log, int argc, char **argv)
{
    boot_log = log;
    first_present_logged = false;
    first_frame_admitted = false;
    first_frame_submitted = false;
    std::set_terminate(report_terminate);
    aurora::fatal_handler = aurora_fatal;
    aurora_set_legacy_gx_panic_handler(aurora_gx_panic);
    AuroraConfig config{};
    config.appName = "SwitchWaker";
    config.userPath = "ux0:/data/tww/";
    config.cachePath = config.userPath;
    config.resourcesPath = "app0:/";
    config.desiredBackend = BACKEND_GXM;
    config.windowWidth = SCREEN_WIDTH;
    config.windowHeight = SCREEN_HEIGHT;
    config.mem1Size = 128 * 1024 * 1024;
    config.mem2Size = 16 * 1024 * 1024;
    config.logLevel = LOG_INFO;
    config.logCallback = aurora_log;
    pc_milestone("vita-gxm-init-begin");
    const AuroraInfo info = aurora_initialize(argc, argv, &config);
    if (info.initializationStatus != AURORA_INITIALIZATION_SUCCESS || info.backend != BACKEND_GXM) {
        probe_log(boot_log, "aurora: GXM startup failed: %s\n",
                  info.initializationError ? info.initializationError : "backend unavailable");
        if (boot_log >= 0)
            sceIoSyncByFd(boot_log, 0);
        return -1;
    }
    pc_milestone("vita-gxm-ready");
    return 0;
}

extern "C" void pc_frame_begin()
{
    aurora::VitaHostAllocScope host_alloc;
    const uint64_t admission_start_us = sceKernelGetSystemTimeWide();
    // The game must only submit GX work once GXM admits the frame.
    for (unsigned int attempt = 0; attempt < 2000; ++attempt) {
        const uint64_t events_start_us = sceKernelGetSystemTimeWide();
        aurora_update();
        total_events_us += sceKernelGetSystemTimeWide() - events_start_us;
        tww_vita_settings_frame();
        if (aurora_begin_frame()) {
            if (!first_frame_admitted) {
                first_frame_admitted = true;
                pc_milestone("vita-first-game-frame-begin");
            }
            check_presentation();
            frame_begin_us = sceKernelGetSystemTimeWide();
            total_admission_us += frame_begin_us - admission_start_us;
            tww_vita_autores_note(frame_begin_us, frame_begin_us - admission_start_us);
            return;
        }
        sceKernelDelayThread(1000);
    }
    pc_milestone("vita-gxm-frame-admission-timeout");
    pc_exit(1);
}

extern "C" void pc_frame_end()
{
    aurora::VitaHostAllocScope host_alloc;
    if (!first_frame_submitted)
        pc_milestone("vita-first-frame-submit-begin");
    const uint64_t submit_start_us = sceKernelGetSystemTimeWide();
    total_game_us += submit_start_us - frame_begin_us;
    aurora_end_frame();
    total_submit_us += sceKernelGetSystemTimeWide() - submit_start_us;
    if (!first_frame_submitted) {
        first_frame_submitted = true;
        pc_milestone("vita-first-frame-submit-complete");
    }
    check_presentation();

    static uint64_t last_report = sceKernelGetSystemTimeWide();
    static unsigned int submitted, last_presented;
    ++submitted;
    const uint64_t now = sceKernelGetSystemTimeWide();
    if (now - last_report >= 10000000) {
        const unsigned int presented = aurora_vita_presented_frames();
        probe_log(boot_log, "vita-frames elapsed_ms=%lu submitted=%u presented=%u\n",
                  static_cast<unsigned long>((now - last_report) / 1000), submitted, presented - last_presented);
        probe_log(boot_log, "vita-frame-average-ms admission=%lu game=%lu submit=%lu\n",
                  static_cast<unsigned long>(total_admission_us / submitted / 1000),
                  static_cast<unsigned long>(total_game_us / submitted / 1000),
                  static_cast<unsigned long>(total_submit_us / submitted / 1000));
        static uint64_t previous_staging_us, previous_finish_us;
        uint64_t staging_us, finish_us;
        tww_vita_gxm_wait_totals(&staging_us, &finish_us);
        probe_log(boot_log, "vita-gxm-wait-ms events=%lu staging=%lu finish=%lu\n",
                  static_cast<unsigned long>(total_events_us / submitted / 1000),
                  static_cast<unsigned long>((staging_us - previous_staging_us) / submitted / 1000),
                  static_cast<unsigned long>((finish_us - previous_finish_us) / submitted / 1000));
        previous_staging_us = staging_us;
        previous_finish_us = finish_us;

        // Where the game thread's frame goes: logic and draw-list encode (game, painter
        // included), the GX encode alone (painter), the end-of-draw wait, GX->GXM translation.
        static unsigned long long previous_phase[7], previous_drain_us;
        unsigned long long phase[7]{};
        tww_vita_perf_totals(phase, 7);
        const unsigned long long drain_us = aurora_vita_fifo_drain_us();
        const auto per_frame = [&](unsigned i) {
            return static_cast<unsigned long>((phase[i] - previous_phase[i]) / submitted / 100);
        };
        const AuroraStats *stats = aurora_get_stats();
        probe_log(boot_log, "vita-phase-0.1ms game=%lu painter=%lu after_draw=%lu audio=%lu pad=%lu drains=%lu draws=%u\n",
                  per_frame(2), per_frame(3), per_frame(6), per_frame(1), per_frame(0),
                  static_cast<unsigned long>((drain_us - previous_drain_us) / submitted),
                  stats ? static_cast<unsigned int>(stats->drawCallCount) : 0u);
        if (aurora_vita_gx_thread()) {
            // Worker busy time, the game thread's waits for it, and hand-offs per frame.
            static unsigned long long previous_worker, previous_wait, previous_publishes;
            unsigned long long worker, wait, publishes;
            aurora_vita_gx_thread_totals(&worker, &wait, &publishes);
            probe_log(boot_log, "vita-gx-thread-0.1ms worker=%lu game_wait=%lu publishes=%lu\n",
                      static_cast<unsigned long>((worker - previous_worker) / submitted / 100),
                      static_cast<unsigned long>((wait - previous_wait) / submitted / 100),
                      static_cast<unsigned long>((publishes - previous_publishes) / submitted));
            previous_worker = worker;
            previous_wait = wait;
            previous_publishes = publishes;
        }
        for (unsigned i = 0; i < 7; ++i)
            previous_phase[i] = phase[i];
        previous_drain_us = drain_us;
        total_admission_us = total_game_us = total_submit_us = total_events_us = 0;
        last_report = now;
        last_presented = presented;
        submitted = 0;
    }
}
