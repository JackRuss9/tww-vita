#include "gpu.h"
#include "vita_dvd_platform.h"
#include "pc_internal.h"
#include "cos_sdk/hooks.h"
#include "m_Do/m_Do_audio.h"
#include "JSystem/JKernel/JKRHeap.h"

#include <aurora/dvd.h>
#include <dolphin/os.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern "C" {
unsigned int _newlib_heap_size_user = 96 * 1024 * 1024;
unsigned int _pthread_stack_default_user = 1024 * 1024;
unsigned int _sceUserMainThreadStackSize = 2 * 1024 * 1024;
}

extern "C" int aurora_main(int argc, char **argv);
extern "C" int tww_vita_gxm_init(SceUID log, int argc, char **argv);

namespace {
constexpr char DataDir[] = "ux0:data/tww";
constexpr char DiscPath[] = "ux0:data/tww/WindWaker.ciso";
SceUID game_log = -1;

void *thread_launch(OSThread *) { return JKRHeap::getCurrentHeap(); }

void thread_start(OSThread *, void *heap)
{
    JKRHeap::sCurrentHeap = static_cast<JKRHeap *>(heap);
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

extern "C" void pc_milestone(const char *name)
{
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

extern "C" void pc_harness_init(int, char **)
{
    pc::gConfig.disc = DiscPath;
    pc::gConfig.audio = false;
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

int main(int argc, char **argv)
{
    sceIoMkdir(DataDir, 0777);
    game_log = sceIoOpen("ux0:data/tww/boot.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    pc_milestone("vita-executable-entered");
    pc_milestone("vita-build-" TWW_VITA_GAME_VERSION);
    pc_exit(aurora_main(argc, argv));
}
