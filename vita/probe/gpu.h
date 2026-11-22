#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2/types.h>

enum { SCREEN_WIDTH = 960, SCREEN_HEIGHT = 544 };

typedef struct {
    uint32_t *pixels;
    unsigned int width, height, stride;
} ProbeImage;

#ifdef __cplusplus
extern "C" {
#endif
int probe_decode_bti(SceUID log, const uint8_t *data, size_t size, ProbeImage *image);
int boot_read_title(SceUID log, const char *path, ProbeImage *image);
void probe_log(SceUID log, const char *format, ...) __attribute__((format(printf, 2, 3)));
int gpu_probe(SceUID log, uint32_t *display, size_t display_size);
int gpu_title(SceUID log, uint32_t *display, size_t display_size, const ProbeImage *image);
#ifdef __cplusplus
}
#endif
