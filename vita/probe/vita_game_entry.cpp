#include <psp2/kernel/threadmgr.h>
#include "gpu.h"
#include "vita_dvd_platform.h"
#include "pc_internal.h"
#include "pc/pc_qol.h"
#include "cos_sdk/hooks.h"
#include "m_Do/m_Do_audio.h"
#include "JSystem/JKernel/JKRHeap.h"

#include <aurora/dvd.h>
#include <dolphin/os.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
unsigned int _newlib_heap_size_user = 96 * 1024 * 1024;
unsigned int _pthread_stack_default_user = 1024 * 1024;
unsigned int _sceUserMainThreadStackSize = 2 * 1024 * 1024;
}

extern "C" int aurora_main(int argc, char **argv);
extern "C" void tww_vita_settings_load();
extern "C" void tww_vita_profiler_start(void) __attribute__((weak));
extern "C" int tww_vita_gxm_init(SceUID log, int argc, char **argv);

namespace {
constexpr char DataDir[] = "ux0:data/tww";
constexpr char DiscPath[] = "ux0:data/tww/WindWaker.ciso";
SceUID game_log = -1;


extern "C" unsigned tww_vita_helper_cpu_mask();

void *thread_launch(OSThread *) { return JKRHeap::getCurrentHeap(); }

void thread_start(OSThread *, void *heap)
{
    JKRHeap::sCurrentHeap = static_cast<JKRHeap *>(heap);
    // The game's own threads (audio, DVD, decompression, card) may run on any core:
    // they would otherwise inherit the game thread's core 0. Pinned to one core
    // together (01.60) the audio threads' DSP boot hung. Cores 0 and 2 (01.70):
    // core 1 is the GX worker's alone.
    // 01.95: the helper cores of CORE LAYOUT (vita_cores.cpp).
    sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), int(tww_vita_helper_cpu_mask()));
}

void logv(SceUID log, const char *format, va_list arguments)
{
    char message[768];
    const int length = std::vsnprintf(message, sizeof(message), format, arguments);
    if (log < 0 || length <= 0)
        return;
    const size_t count = static_cast<size_t>(length) < sizeof(message)
        ? static_cast<size_t>(length) : sizeof(message) - 1;
    sceIoWrite(log, message, count);
}

// Homebrew starts at 333 MHz CPU and 111 MHz GPU; the game needs the maximum.
void raise_clocks(SceUID log)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    char message[160];
    const int length = std::snprintf(message, sizeof(message),
        "vita-clocks arm=%d bus=%d gpu=%d xbar=%d\n",
        scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(),
        scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency());
    if (log >= 0 && length > 0)
        sceIoWrite(log, message, static_cast<SceSize>(length));
}
}

void probe_log(SceUID log, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    logv(log, format, arguments);
    va_end(arguments);
}

namespace pc {
Config gConfig;

void writef(int, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    logv(game_log, format, arguments);
    va_end(arguments);
}

void discMessage(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    logv(game_log, format, arguments);
    va_end(arguments);
    probe_log(game_log, "\n");
}

[[noreturn]] void exitDisc() { pc_exit(PC_EXIT_DISC); }
}

extern "C" void tww_vita_watchdog_note(const char *milestone);

extern "C" void pc_milestone(const char *name)
{
    tww_vita_watchdog_note(name);
    probe_log(game_log, "%s\n", name);
    if (game_log >= 0)
        sceIoSyncByFd(game_log, 0);
}

extern "C" void probe_scheduler_diagnostic(const char *message, bool fatal)
{
    probe_log(game_log, "scheduler%s: %s\n", fatal ? " fatal" : "", message);
    if (fatal && game_log >= 0)
        sceIoSyncByFd(game_log, 0);
}

extern "C" void tww_vita_settings_load();
extern "C" void tww_vita_aspect_setup();
extern "C" void tww_vita_cores_setup();
extern "C" bool tww_vita_audio_muted();
extern "C" bool tww_vita_skip_intro();

extern "C" void pc_harness_init(int, char **)
{
    // Settings first: COS_ASPECT must be set before the game reads it.
    tww_vita_settings_load();
    tww_vita_cores_setup();  // before any helper thread starts
    tww_vita_aspect_setup();
    pc::gConfig.audio = !tww_vita_audio_muted();
    pc::gConfig.disc = DiscPath;
    // The legend and the Outset prologue play unless SKIP INTRO is on.
    pc_qol_set("COS_QOL_SKIP_INTRO", tww_vita_skip_intro() ? "1" : "0");
    pc_milestone(tww_vita_skip_intro() ? "vita-opening-legend-skip-enabled" : "vita-opening-legend-plays");
}

extern "C" OSThread *pc_main_thread()
{
    return COSSdkGetDefaultThread();
}

extern "C" void pc_aurora_init(int argc, char **argv)
{
    if (tww_vita_gxm_init(game_log, argc, argv) != 0)
        pc_exit(PC_EXIT_USAGE);
    tww_vita_set_dvd_log(game_log);
    pc_milestone("vita-opening-disc");
    if (!aurora_dvd_open(DiscPath))
        pc::exitDisc();
    pc_milestone("vita-loading-disc-assets");
    pc::loadAssets();
    OSInit();
    COSSdkSetThreadLaunchHook(thread_launch);
    COSSdkSetThreadStartHook(thread_start);
    // Muted: the game sees its audio as ready and never starts JAudio.
    if (tww_vita_audio_muted())
        mDoAud_zelAudio_c::onInitFlag();
    pc_milestone("vita-platform-ready");
}

extern "C" [[noreturn]] void pc_exit(int status)
{
    probe_log(game_log, "vita-process-exit %d\n", status);
    if (game_log >= 0)
        sceIoSyncByFd(game_log, 0);
    sceKernelExitProcess(status);
    std::abort();
}

extern "C" void tww_vita_begin_host_alloc() { JKRPcBeginHostAlloc(); }
extern "C" void tww_vita_end_host_alloc() { JKRPcEndHostAlloc(); }

int main(int argc, char **argv)
{
    sceIoMkdir(DataDir, 0777);
    game_log = sceIoOpen("ux0:data/tww/boot.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    pc_milestone("vita-executable-entered");
    pc_milestone("vita-build-" TWW_VITA_GAME_VERSION);
    raise_clocks(game_log);
    if (tww_vita_profiler_start)
        tww_vita_profiler_start();
    pc_exit(aurora_main(argc, argv));
}

// cos_sdk's AR/AI/DSP: MEM1's size as configured in tww_vita_gxm_init (mh3 Aurora's
// AuroraConfig differs from the headers cos_sdk is compiled with).
extern "C" uint32_t tww_vita_mem1_size() { return 128u * 1024u * 1024u; }
