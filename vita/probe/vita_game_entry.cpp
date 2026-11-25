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
extern "C" void aurora_vita_set_efb_scale(unsigned percent);
extern "C" void aurora_vita_set_float_tev(int enabled);
extern "C" int tww_vita_gxm_init(SceUID log, int argc, char **argv);

namespace {
constexpr char DataDir[] = "ux0:data/tww";
constexpr char DiscPath[] = "ux0:data/tww/WindWaker.ciso";
constexpr char SettingsPath[] = "ux0:data/tww/settings.txt";
SceUID game_log = -1;

struct Settings {
    unsigned resolution = 50; // percent of the GX EFB in each dimension
    int float_tev = 1;        // float TEV combiners instead of exact 8-bit emulation
};

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

void write_default_settings(const Settings &settings)
{
    const SceUID file = sceIoOpen(SettingsPath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (file < 0)
        return;
    char text[512];
    const int length = std::snprintf(text, sizeof(text),
        "# SwitchWaker Vita settings (read at start-up)\n"
        "# resolution: percent of the GameCube frame in each dimension, 25-100\n"
        "resolution=%u\n"
        "# float_tev: 1 = fast float colour combiners, 0 = exact 8-bit emulation (slow)\n"
        "float_tev=%d\n",
        settings.resolution, settings.float_tev);
    if (length > 0)
        sceIoWrite(file, text, static_cast<SceSize>(length));
    sceIoClose(file);
}

Settings load_settings()
{
    Settings settings;
    const SceUID file = sceIoOpen(SettingsPath, SCE_O_RDONLY, 0);
    if (file < 0) {
        write_default_settings(settings);
        return settings;
    }
    char text[2048];
    const int length = sceIoRead(file, text, sizeof(text) - 1);
    sceIoClose(file);
    text[length > 0 ? length : 0] = '\0';
    for (char *line = text; line && *line;) {
        char *next = std::strchr(line, '\n');
        if (next)
            *next++ = '\0';
        unsigned value;
        if (std::sscanf(line, " resolution = %u", &value) == 1)
            settings.resolution = value < 25 ? 25 : value > 100 ? 100 : value;
        else if (std::sscanf(line, " float_tev = %u", &value) == 1)
            settings.float_tev = value != 0;
        line = next;
    }
    return settings;
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
    pc_qol_set("COS_QOL_SKIP_INTRO", "1");
    pc_milestone("vita-opening-legend-skip-enabled");
}

extern "C" OSThread *pc_main_thread()
{
    return COSSdkGetDefaultThread();
}

extern "C" void pc_aurora_init(int argc, char **argv)
{
    const Settings settings = load_settings();
    probe_log(game_log, "vita-settings resolution=%u float_tev=%d\n", settings.resolution, settings.float_tev);
    aurora_vita_set_efb_scale(settings.resolution);
    aurora_vita_set_float_tev(settings.float_tev);
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

extern "C" void tww_vita_begin_host_alloc() { JKRPcBeginHostAlloc(); }
extern "C" void tww_vita_end_host_alloc() { JKRPcEndHostAlloc(); }

int main(int argc, char **argv)
{
    sceIoMkdir(DataDir, 0777);
    game_log = sceIoOpen("ux0:data/tww/boot.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    pc_milestone("vita-executable-entered");
    pc_milestone("vita-build-" TWW_VITA_GAME_VERSION);
    raise_clocks(game_log);
    pc_exit(aurora_main(argc, argv));
}
