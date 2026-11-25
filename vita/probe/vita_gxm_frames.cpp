#include "gpu.h"
#include "pc/pc_harness.h"

#include <cstdlib>
#include <exception>

#include <aurora/aurora.h>
#include <aurora/fatal.hpp>
#include <aurora/legacy_gx.h>
#include <aurora/vita_host_alloc.hpp>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>

extern "C" unsigned int aurora_vita_presented_frames();

namespace {
SceUID boot_log = -1;
bool first_present_logged;
bool first_frame_admitted;
bool first_frame_submitted;
uint64_t frame_begin_us, total_admission_us, total_game_us, total_submit_us;

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
        aurora_update();
        if (aurora_begin_frame()) {
            if (!first_frame_admitted) {
                first_frame_admitted = true;
                pc_milestone("vita-first-game-frame-begin");
            }
            check_presentation();
            frame_begin_us = sceKernelGetSystemTimeWide();
            total_admission_us += frame_begin_us - admission_start_us;
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
        total_admission_us = total_game_us = total_submit_us = 0;
        last_report = now;
        last_presented = presented;
        submitted = 0;
    }
}
