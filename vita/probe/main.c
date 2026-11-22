#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/display.h>
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

int main(void)
{
    sceIoMkdir(DATA_DIR, 0777);
    SceUID log = sceIoOpen(DATA_DIR "/probe.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    probe_log(log, "TWW Vita hardware probe 7\n");
    probe_log(log, "ABI: %u-bit pointers\n", (unsigned int)(8 * sizeof(void *)));
    int boot_result = probe_boot_disc(log, DISC_PATH);
    probe_log(log, "boot: disc startup data %s\n", boot_result == 0 ? "pass" : "failed");
    int display = show_result(log, boot_result);
    int sampling = sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    probe_log(log, "pad: analog mode 0x%08x\n", (unsigned int)sampling);

    SceCtrlData pad;
    int samples = sceCtrlPeekBufferPositive(0, &pad, 1);
    probe_log(log, "pad: initial sample %d\n", samples);
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
    return boot_result < 0 || display < 0 || sampling < 0 || log < 0;
}
