#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2/types.h>

enum { SCREEN_WIDTH = 960, SCREEN_HEIGHT = 544 };

#ifdef __cplusplus
extern "C" {
#endif
void probe_log(SceUID log, const char *format, ...) __attribute__((format(printf, 2, 3)));
int gpu_probe(SceUID log, uint32_t *display, size_t display_size);
#ifdef __cplusplus
}
#endif
