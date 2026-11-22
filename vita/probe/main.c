#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

#include "disc.h"
#include "gpu.h"

#define DATA_DIR "ux0:data/tww"
#define DISC_PATH DATA_DIR "/WindWaker.ciso"

enum {
    VIDEO_MEMORY_SIZE = 2 * 1024 * 1024,
};

static uint32_t banner[BOOT_BANNER_WIDTH * BOOT_BANNER_HEIGHT];
static int banner_ready;
static ProbeImage title;
static SceUID title_memory = -1;
static uint32_t *title_frame;

void probe_log(SceUID log, const char *format, ...)
{
    if (log < 0)
        return;

    char line[192];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length > 0) {
        size_t remaining = (size_t)length < sizeof(line) ? (size_t)length : sizeof(line) - 1;
        const char *position = line;
        while (remaining > 0) {
            int written = sceIoWrite(log, position, remaining);
            if (written <= 0)
                break;
            remaining -= (size_t)written;
            position += written;
        }
    }
}

static void checkpoint(SceUID log, const char *name, const char *result)
{
    probe_log(log, "check: %s %s\n", name, result);
    if (log >= 0)
        sceIoSyncByFd(log, 0);
}

static void display_queue_callback(const void *data)
{
    (void)data;
}

static int check_graphics(SceUID log)
{
    const SceGxmInitializeParams config = {
        .flags = SCE_GXM_INITIALIZE_FLAG_DEFAULT,
        .displayQueueMaxPendingCount = 2,
        .displayQueueCallback = display_queue_callback,
        .displayQueueCallbackDataSize = sizeof(uint32_t),
        .parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE,
    };
    int result = sceGxmInitialize(&config);
    probe_log(log, "gpu: initialization 0x%08x\n", (unsigned int)result);
    if (result < 0) {
        checkpoint(log, "GXM texture and framebuffer", "FAIL (GXM initialization)");
        checkpoint(log, "title texture render", "SKIP (GXM initialization failed)");
        return -1;
    }

    SceUID memory = sceKernelAllocMemBlock("twwSuiteGxmOutput",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, VIDEO_MEMORY_SIZE, NULL);
    if (memory >= 0) {
        void *base;
        result = sceKernelGetMemBlockBase(memory, &base);
        if (result >= 0)
            result = gpu_probe(log, base, VIDEO_MEMORY_SIZE);
        sceKernelFreeMemBlock(memory);
    } else {
        result = memory;
    }
    checkpoint(log, "GXM texture and framebuffer", result >= 0 ? "PASS" : "FAIL");

    int title_result = 0;
    if (title.pixels) {
        checkpoint(log, "title texture render", "BEGIN");
        title_memory = sceKernelAllocMemBlock("twwTitleGxmOutput",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, VIDEO_MEMORY_SIZE, NULL);
        title_result = title_memory;
        if (title_memory >= 0) {
            void *base = NULL;
            title_result = sceKernelGetMemBlockBase(title_memory, &base);
            if (title_result >= 0)
                title_result = gpu_title(log, base, VIDEO_MEMORY_SIZE, &title);
            if (title_result >= 0)
                title_frame = base;
            else
                sceKernelFreeMemBlock(title_memory);
        }
        checkpoint(log, "title texture render", title_result >= 0 ? "PASS" : "FAIL");
    } else {
        checkpoint(log, "title texture render", "SKIP (title texture unavailable)");
    }
    int shutdown = sceGxmTerminate();
    probe_log(log, "gpu: render/copy 0x%08x, termination 0x%08x\n",
              (unsigned int)result, (unsigned int)shutdown);
    return result < 0 || title_result < 0 || shutdown < 0 ? -1 : 0;
}

static int show_result(SceUID log, int boot_result)
{
    SceUID memory = sceKernelAllocMemBlock("twwProbeFramebuffer",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, VIDEO_MEMORY_SIZE, NULL);
    if (memory < 0) {
        probe_log(log, "display: memory 0x%08x\n", (unsigned int)memory);
        return memory;
    }

    void *base;
    int result = sceKernelGetMemBlockBase(memory, &base);
    if (result < 0) {
        probe_log(log, "display: base 0x%08x\n", (unsigned int)result);
        sceKernelFreeMemBlock(memory);
        return result;
    }

    uint32_t *pixels = base;
    const uint32_t color = boot_result == 0 ? 0xff358220 : 0xff2630a0;
    for (unsigned int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        pixels[i] = color;
    if (title_frame) {
        memcpy(pixels + 8 * SCREEN_WIDTH, title_frame + 8 * SCREEN_WIDTH,
               (SCREEN_HEIGHT - 16) * SCREEN_WIDTH * sizeof(*pixels));
        sceKernelFreeMemBlock(title_memory);
    } else if (banner_ready) {
        enum { SCALE = 5, LEFT = (SCREEN_WIDTH - BOOT_BANNER_WIDTH * SCALE) / 2,
               TOP = (SCREEN_HEIGHT - BOOT_BANNER_HEIGHT * SCALE) / 2 };
        for (unsigned int y = 0; y < BOOT_BANNER_HEIGHT * SCALE; ++y)
            for (unsigned int x = 0; x < BOOT_BANNER_WIDTH * SCALE; ++x)
                pixels[(TOP + y) * SCREEN_WIDTH + LEFT + x] =
                    banner[(y / SCALE) * BOOT_BANNER_WIDTH + x / SCALE];
    }

    SceDisplayFrameBuf frame = {
        .size = sizeof(frame),
        .base = base,
        .pitch = SCREEN_WIDTH,
        .pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT,
    };
    result = sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
    probe_log(log, "display: set framebuffer 0x%08x\n", (unsigned int)result);
    if (result < 0)
        sceKernelFreeMemBlock(memory);
    // A displayed block remains owned by the process until exit.
    return result;
}

void boot_probe_exit(SceUID log, int status)
{
    int sampling = sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    probe_log(log, "pad: analog mode 0x%08x\n", (unsigned int)sampling);

    SceCtrlData pad;
    int samples = sceCtrlPeekBufferPositive(0, &pad, 1);
    probe_log(log, "pad: initial sample %d\n", samples);
    checkpoint(log, "controller", sampling >= 0 && samples > 0 ? "PASS" : "FAIL");
    status |= sampling < 0 || samples <= 0 || log < 0;
    int display = show_result(log, status);
    checkpoint(log, "display", display >= 0 ? "PASS" : "FAIL");
    checkpoint(log, "suite", status == 0 && display >= 0 ? "PASS" : "FAIL");
    probe_log(log, "ready: press START to exit\n");

    for (unsigned int attempt = 0; attempt < 1800; ++attempt) {
        samples = sceCtrlPeekBufferPositive(0, &pad, 1);
        if (samples > 0 && (pad.buttons & SCE_CTRL_START)) {
            probe_log(log, "pad: START detected; sticks %u,%u / %u,%u\n",
                     (unsigned int)pad.lx, (unsigned int)pad.ly,
                     (unsigned int)pad.rx, (unsigned int)pad.ry);
            break;
        }
        sceKernelDelayThread(16667);
    }
    probe_log(log, "done\n");
    if (log >= 0)
        sceIoClose(log);
    exit(status != 0 || display < 0 || sampling < 0 || log < 0);
}

int main(void)
{
    sceIoMkdir(DATA_DIR, 0777);
    SceUID log = sceIoOpen(DATA_DIR "/probe.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    probe_log(log, "TWW Vita hardware probe 12 (scheduler)\n");
    probe_log(log, "ABI: %u-bit pointers\n", (unsigned int)(8 * sizeof(void *)));

    checkpoint(log, "disc mount", "BEGIN");
    BootDisc *disc = boot_disc_open(log, DISC_PATH);
    int disc_ok = disc != NULL;
    if (disc)
        boot_disc_close(disc);
    checkpoint(log, "disc mount", disc_ok ? "PASS" : "FAIL");

    int failed = !disc_ok;
    if (disc_ok) {
        checkpoint(log, "SwitchWaker assets", "BEGIN");
        int result = boot_switchwaker_assets(log, DISC_PATH);
        checkpoint(log, "SwitchWaker assets", result == 0 ? "PASS" : "FAIL");
        failed |= result != 0;

        checkpoint(log, "title and ocean archives", "BEGIN");
        result = boot_switchwaker_archives(log, DISC_PATH);
        checkpoint(log, "title and ocean archives", result == 0 ? "PASS" : "FAIL");
        failed |= result != 0;

        checkpoint(log, "disc banner", "BEGIN");
        banner_ready = boot_read_banner(log, DISC_PATH, banner) == 0;
        checkpoint(log, "disc banner", banner_ready ? "PASS" : "FAIL");
        failed |= !banner_ready;
        checkpoint(log, "title texture load", "BEGIN");
        result = boot_read_title(log, DISC_PATH, &title);
        checkpoint(log, "title texture load", result == 0 ? "PASS" : "FAIL");
        failed |= result != 0;
    } else {
        checkpoint(log, "SwitchWaker assets", "SKIP (disc mount failed)");
        checkpoint(log, "title and ocean archives", "SKIP (disc mount failed)");
        checkpoint(log, "disc banner", "SKIP (disc mount failed)");
        checkpoint(log, "title texture load", "SKIP (disc mount failed)");
    }

    checkpoint(log, "Aurora OS and JKR heaps", "BEGIN");
    int result = boot_probe_system(log);
    checkpoint(log, "Aurora OS and JKR heaps", result == 0 ? "PASS" : "FAIL");
    failed |= result != 0;
    if (result == 0) {
        checkpoint(log, "SwitchWaker scheduler", "BEGIN");
        result = boot_probe_scheduler(log);
        checkpoint(log, "SwitchWaker scheduler", result == 0 ? "PASS" : "FAIL");
        failed |= result != 0;
    } else {
        checkpoint(log, "SwitchWaker scheduler", "SKIP (heaps unavailable)");
    }

    checkpoint(log, "GXM texture and framebuffer", "BEGIN");
    result = check_graphics(log);
    checkpoint(log, "GXM suite", result == 0 ? "PASS" : "FAIL");
    free(title.pixels);
    boot_probe_exit(log, failed || result != 0);
}
