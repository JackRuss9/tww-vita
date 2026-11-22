#include "gpu.h"

#include <stdlib.h>

static unsigned int be16(const uint8_t *p)
{
    return (unsigned int)p[0] << 8 | p[1];
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)be16(p) << 16 | be16(p + 2);
}

static uint32_t color(unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
    return 0xff000000u | (b * a / 255) << 16 | (g * a / 255) << 8 | r * a / 255;
}

static uint32_t rgb565(unsigned int v)
{
    return color((v >> 11) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31, 255);
}

static uint32_t rgb5a3(unsigned int v)
{
    if (v & 0x8000)
        return color(((v >> 10) & 31) * 255 / 31, ((v >> 5) & 31) * 255 / 31, (v & 31) * 255 / 31, 255);
    return color(((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, (v >> 12) * 255 / 7);
}

static uint32_t cmpr_texel(const uint8_t *tile, unsigned int x, unsigned int y)
{
    const uint8_t *block = tile + (y / 4 * 2 + x / 4) * 8;
    unsigned int a = be16(block), b = be16(block + 2);
    uint32_t c0 = rgb565(a), c1 = rgb565(b);
    unsigned int index = (block[4 + y % 4] >> (6 - 2 * (x % 4))) & 3;
    if (index == 0)
        return c0;
    if (index == 1)
        return c1;
    if (a <= b && index == 3)
        return 0xff000000;
    unsigned int factor = a <= b ? 1 : 4 - index;
    unsigned int divisor = a <= b ? 2 : 3;
    uint32_t result = 0xff000000;
    for (unsigned int shift = 0; shift < 24; shift += 8) {
        unsigned int first = (c0 >> shift) & 255, second = (c1 >> shift) & 255;
        result |= ((first * factor + second * (divisor - factor)) / divisor) << shift;
    }
    return result;
}

int probe_decode_bti(SceUID log, const uint8_t *data, size_t size, ProbeImage *image)
{
    static const uint8_t tile_w[] = {8,8,8,4,4,4,4,0,8,8,4,0,0,0,8};
    static const uint8_t tile_h[] = {8,4,4,4,4,4,4,0,8,4,4,0,0,0,8};
    if (size < 32)
        return -1;
    unsigned int format = data[0], w = be16(data + 2), h = be16(data + 4);
    probe_log(log, "title: BTI format %u, %ux%u\n", format, w, h);
    if (format >= sizeof(tile_w) || !tile_w[format] || !w || !h || w > 1024 || h > 1024)
        return -1;
    unsigned int tw = tile_w[format], th = tile_h[format], bytes = format == 6 ? 64 : 32;
    size_t cols = (w + tw - 1) / tw, rows = (h + th - 1) / th;
    uint32_t offset = be32(data + 28);
    if (offset < 32 || offset > size || cols * rows * bytes > size - offset)
        return -1;
    unsigned int palette = be16(data + 10), palette_format = data[9];
    uint32_t palette_offset = be32(data + 12);
    if (format >= 8 && format <= 10 &&
        (palette_format > 2 || palette == 0 || palette_offset < 32 ||
         palette_offset > size || palette * 2 > size - palette_offset))
        return -1;
    unsigned int stride = (w + 7) & ~7u;
    uint32_t *pixels = calloc(stride * h, sizeof(*pixels));
    if (!pixels)
        return -1;
    for (unsigned int y = 0; y < h; ++y) {
        for (unsigned int x = 0; x < w; ++x) {
            const uint8_t *t = data + offset + (y / th * cols + x / tw) * bytes;
            unsigned int i = y % th * tw + x % tw;
            unsigned int nibble = (t[i / 2] >> (i % 2 == 0 ? 4 : 0)) & 15;
            uint32_t rgba;
            switch (format) {
            case 0: rgba = color(nibble * 17, nibble * 17, nibble * 17, 255); break;
            case 1: rgba = color(t[i], t[i], t[i], 255); break;
            case 2: rgba = color((t[i]&15)*17, (t[i]&15)*17, (t[i]&15)*17, (t[i]>>4)*17); break;
            case 3: rgba = color(t[2*i+1], t[2*i+1], t[2*i+1], t[2*i]); break;
            case 4: rgba = rgb565(be16(t + 2*i)); break;
            case 5: rgba = rgb5a3(be16(t + 2*i)); break;
            case 6: rgba = color(t[2*i+1], t[32+2*i], t[33+2*i], t[2*i]); break;
            case 14: rgba = cmpr_texel(t, x % tw, y % th); break;
            default: {
                unsigned int idx = format == 8 ? nibble : format == 9 ? t[i] : be16(t + 2*i) & 0x3fff;
                if (idx >= palette) {
                    free(pixels);
                    return -1;
                }
                const uint8_t *p = data + palette_offset + 2*idx;
                rgba = palette_format == 0 ? color(p[1], p[1], p[1], p[0]) :
                    palette_format == 1 ? rgb565(be16(p)) : rgb5a3(be16(p));
                break;
            }
            }
            pixels[y * stride + x] = rgba;
        }
    }
    *image = (ProbeImage){pixels, w, h, stride};
    return 0;
}
