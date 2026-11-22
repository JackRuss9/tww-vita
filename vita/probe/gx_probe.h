#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    float x, y, z;
    uint32_t rgba;
} GxProbeVertex;

typedef struct {
    GxProbeVertex vertices[7];
    uint16_t indices[9];
    size_t fifo_bytes;
    uint16_t vertex_count;
    uint16_t index_count;
} GxProbeMesh;

#ifdef __cplusplus
extern "C" {
#endif
int gx_probe_mesh(GxProbeMesh *mesh);
#ifdef __cplusplus
}
#endif
