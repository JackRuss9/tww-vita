#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

#define DATA_DIR "ux0:data/tww-vita"
#define DISC_PATH DATA_DIR "/disc.iso"

enum {
    SCREEN_WIDTH = 960,
    SCREEN_HEIGHT = 544,
    VIDEO_MEMORY_SIZE = 2 * 1024 * 1024,
};

static void log_line(SceUID log, const char *format, ...)
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

static void probe_disc(SceUID log)
{
    SceUID file = sceIoOpen(DISC_PATH, SCE_O_RDONLY, 0);
    if (file < 0) {
        log_line(log, "disc: open 0x%08x (optional)\n", (unsigned int)file);
        return;
    }

    unsigned char header[0x20];
    int read = sceIoRead(file, header, sizeof(header));
    SceOff size = sceIoLseek(file, 0, SCE_SEEK_END);
    if (read != sizeof(header)) {
        log_line(log, "disc: header read %d\n", read);
    } else if (memcmp(header + 0x1c, "\xc2\x33\x9f\x3d", 4) != 0) {
        log_line(log, "disc: expected a raw GameCube ISO\n");
    } else if (memcmp(header, "GZLE01", 6) == 0 || memcmp(header, "GZLP01", 6) == 0) {
        log_line(log, "disc: %.6s revision %u size %lld\n", (const char *)header,
                 (unsigned int)header[7], (long long)size);
    } else {
        log_line(log, "disc: unsupported GameCube disc ID\n");
    }
    sceIoClose(file);
}

static int show_result(SceUID log, uint32_t color)
{
    SceUID memory = sceKernelAllocMemBlock("twwProbeFramebuffer",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, VIDEO_MEMORY_SIZE, NULL);
    if (memory < 0) {
        log_line(log, "display: memory 0x%08x\n", (unsigned int)memory);
        return memory;
    }

    void *base;
    int result = sceKernelGetMemBlockBase(memory, &base);
    if (result < 0) {
        log_line(log, "display: base 0x%08x\n", (unsigned int)result);
        sceKernelFreeMemBlock(memory);
        return result;
    }

    uint32_t *pixels = base;
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
    log_line(log, "display: set framebuffer 0x%08x\n", (unsigned int)result);
    if (result < 0)
        sceKernelFreeMemBlock(memory);
    // A displayed block remains owned by the process until exit.
    return result;
}

static void display_queue_callback(const void *data)
{
    (void)data;
}

int main(void)
{
    sceIoMkdir(DATA_DIR, 0777);
    SceUID log = sceIoOpen(DATA_DIR "/probe.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    log_line(log, "TWW Vita hardware probe 1\n");
    log_line(log, "ABI: %u-bit pointers\n", (unsigned int)(8 * sizeof(void *)));
    probe_disc(log);

    const SceGxmInitializeParams gxm = {
        .flags = SCE_GXM_INITIALIZE_FLAG_DEFAULT,
        .displayQueueMaxPendingCount = 2,
        .displayQueueCallback = display_queue_callback,
        .displayQueueCallbackDataSize = sizeof(uint32_t),
        .parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE,
    };
    log_line(log, "gxm: initializing\n");
    int graphics = sceGxmInitialize(&gxm);
    log_line(log, "gxm: initialize 0x%08x\n", (unsigned int)graphics);

    uint32_t color = (graphics >= 0 && log >= 0) ? 0xff358220 : 0xff2630a0;
    int display = show_result(log, color);
    int sampling = sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    log_line(log, "pad: analog mode 0x%08x\n", (unsigned int)sampling);

    SceCtrlData pad;
    int samples = sceCtrlPeekBufferPositive(0, &pad, 1);
    log_line(log, "pad: initial sample %d\n", samples);
    log_line(log, "ready: press START to exit\n");

    for (unsigned int attempt = 0; attempt < 1800; ++attempt) {
        samples = sceCtrlPeekBufferPositive(0, &pad, 1);
        if (samples > 0 && (pad.buttons & SCE_CTRL_START)) {
            log_line(log, "pad: START detected; sticks %u,%u / %u,%u\n",
                     (unsigned int)pad.lx, (unsigned int)pad.ly,
                     (unsigned int)pad.rx, (unsigned int)pad.ry);
            break;
        }
        sceKernelDelayThread(16667);
    }
    if (graphics >= 0)
        log_line(log, "gxm: terminate 0x%08x\n", (unsigned int)sceGxmTerminate());
    log_line(log, "done\n");
    if (log >= 0)
        sceIoClose(log);
    return graphics < 0 || display < 0 || sampling < 0 || log < 0;
}
