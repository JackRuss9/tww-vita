#include "gpu.h"
#include "pc/pc_harness.h"

#include <aurora/aurora.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

extern "C" unsigned int aurora_vita_presented_frames();

namespace {
SceUID boot_log = -1;
bool first_present_logged;

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
    // The game must only submit GX work once GXM admits the frame.
    for (unsigned int attempt = 0; attempt < 2000; ++attempt) {
        aurora_update();
        if (aurora_begin_frame())
            return;
        sceKernelDelayThread(1000);
    }
    pc_milestone("vita-gxm-frame-admission-timeout");
    sceKernelExitProcess(1);
}

extern "C" void pc_frame_end()
{
    aurora_end_frame();
    if (!first_present_logged && aurora_vita_presented_frames() != 0) {
        first_present_logged = true;
        pc_milestone("vita-first-frame-presented");
    }
}
