#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    float x, y, z;
    float s, t;
} GxProbeVertex;

typedef struct {
    GxProbeVertex vertices[8];
    uint16_t indices[12];
    size_t fifo_bytes;
    uint16_t vertex_count;
    uint16_t index_count;
} GxProbeMesh;

enum { GX_PROBE_TEXTURE_SIZE = 16 };

#ifdef __cplusplus
extern "C" {
#endif
int gx_probe_mesh(GxProbeMesh *mesh);
void gx_probe_texture(uint32_t *rgba);
#ifdef __cplusplus
}
#endif
