#include "gx_probe.h"
#include "aurora_fifo.hpp"

#include <array>
#include <span>

namespace {
constexpr uint8_t LoadCp = 0x08, Quads = 0x80, Triangles = 0x90;
constexpr uint8_t VcdLow = 0x50, VcdHigh = 0x60, VatA = 0x70;
constexpr uint32_t DirectPosition = 1u << 9, DirectTex0 = 1;
constexpr uint32_t XyzFloatStFloat = 1u | (4u << 1) | (1u << 21) | (4u << 22);
constexpr size_t VertexBytes = 20;
constexpr std::array<uint16_t, 6> QuadIndices = {0, 1, 2, 2, 3, 0};
uint8_t fifo_storage[256];
bool fifo_overflow;
}

namespace aurora::gx::fifo {
namespace detail {
uint8_t *sBufferData = fifo_storage;
uint32_t sBufferSize = 0, sBufferCapacity = sizeof(fifo_storage);
bool sInDisplayList = false;
uint8_t *sDlBuffer = nullptr;
uint32_t sDlSize = 0, sDlWritePos = 0;
}

void write_data_grow(const void *, uint32_t)
{
    fifo_overflow = true;
}
}

namespace {
namespace fifo = aurora::gx::fifo;

void cp_register(uint8_t address, uint32_t value)
{
    fifo::write_u8(LoadCp);
    fifo::write_u8(address);
    fifo::write_u32(value);
}

void vertex(float x, float y, float s, float t)
{
    fifo::write_f32(x);
    fifo::write_f32(y);
    fifo::write_f32(0.5f);
    fifo::write_f32(s);
    fifo::write_f32(t);
}

void record_scene()
{
    cp_register(VcdLow, DirectPosition);
    cp_register(VcdHigh, DirectTex0);
    cp_register(VatA, XyzFloatStFloat);
    fifo::write_u8(Quads); // GX_VTXFMT0
    fifo::write_u16(8);
    constexpr float swatch = 0.5f / static_cast<float>(GX_PROBE_TEXTURE_SIZE);
    vertex(-1.0f, -1.0f, swatch, swatch);
    vertex( 1.0f, -1.0f, swatch, swatch);
    vertex( 1.0f,  1.0f, swatch, swatch);
    vertex(-1.0f,  1.0f, swatch, swatch);
    vertex(-0.6f,  0.6f, 0.0f, 0.0f);
    vertex( 0.6f,  0.6f, 1.0f, 0.0f);
    vertex( 0.6f, -0.6f, 1.0f, 1.0f);
    vertex(-0.6f, -0.6f, 0.0f, 1.0f);
}

uint32_t load_be32(const uint8_t *bytes)
{
    return (uint32_t{bytes[0]} << 24) | (uint32_t{bytes[1]} << 16) |
           (uint32_t{bytes[2]} << 8) | bytes[3];
}

bool decode_scene(std::span<const uint8_t> bytes, GxProbeMesh &mesh)
{
    uint32_t vcd_low = 0, vcd_high = 0, vat_a = 0;
    while (!bytes.empty()) {
        if (bytes.front() == LoadCp) {
            if (bytes.size() < 6)
                return false;
            const uint32_t value = load_be32(bytes.data() + 2);
            switch (bytes[1]) {
            case VcdLow: vcd_low = value; break;
            case VcdHigh: vcd_high = value; break;
            case VatA: vat_a = value; break;
            default: return false;
            }
            bytes = bytes.subspan(6);
            continue;
        }

        const uint8_t primitive = bytes.front();
        if (bytes.size() < 3 || (primitive != Quads && primitive != Triangles) ||
            vcd_low != DirectPosition || vcd_high != DirectTex0 || vat_a != XyzFloatStFloat)
            return false;
        const uint16_t count = (uint16_t{bytes[1]} << 8) | bytes[2];
        const size_t group_size = primitive == Quads ? 4 : 3;
        const size_t required_indices = primitive == Quads ? count / 4 * 6 : count;
        bytes = bytes.subspan(3);
        if (!count || count % group_size || count > bytes.size() / VertexBytes ||
            mesh.vertex_count + count > std::size(mesh.vertices) ||
            mesh.index_count + required_indices > std::size(mesh.indices))
            return false;

        const uint16_t base = mesh.vertex_count;
        for (uint16_t i = 0; i < count; ++i) {
            const auto *data = bytes.data();
            mesh.vertices[mesh.vertex_count++] = {
                std::bit_cast<float>(load_be32(data)),
                std::bit_cast<float>(load_be32(data + 4)),
                std::bit_cast<float>(load_be32(data + 8)),
                std::bit_cast<float>(load_be32(data + 12)),
                std::bit_cast<float>(load_be32(data + 16)),
            };
            bytes = bytes.subspan(VertexBytes);
        }
        if (primitive == Quads) {
            for (uint16_t group = 0; group < count; group += 4)
                for (const uint16_t index : QuadIndices)
                    mesh.indices[mesh.index_count++] = base + group + index;
        } else {
            for (uint16_t index = 0; index < count; ++index)
                mesh.indices[mesh.index_count++] = base + index;
        }
    }
    return mesh.vertex_count == std::size(mesh.vertices) &&
           mesh.index_count == std::size(mesh.indices);
}
}

int gx_probe_mesh(GxProbeMesh *mesh)
{
    *mesh = {};
    fifo::detail::sBufferSize = 0;
    fifo_overflow = false;
    record_scene();
    mesh->fifo_bytes = fifo::detail::sBufferSize;
    return !fifo_overflow && decode_scene({fifo_storage, mesh->fifo_bytes}, *mesh) ? 0 : -1;
}

void gx_probe_texture(uint32_t *rgba)
{
    constexpr unsigned Tile = 4;
    constexpr unsigned Size = GX_PROBE_TEXTURE_SIZE;
    constexpr uint16_t Colors[2][2] = {{0xf800, 0x07e0}, {0x001f, 0xffe0}};
    uint8_t tiled[Size * Size * 2];
    size_t write = 0;
    for (unsigned tile_y = 0; tile_y < Size; tile_y += Tile)
        for (unsigned tile_x = 0; tile_x < Size; tile_x += Tile)
            for (unsigned texel = 0; texel < Tile * Tile; ++texel) {
                const uint16_t color = write == 0 ? 0 : Colors[tile_y / (Size / 2)][tile_x / (Size / 2)];
                tiled[write++] = static_cast<uint8_t>(color >> 8);
                tiled[write++] = static_cast<uint8_t>(color);
            }

    // GX RGB565 packs big-endian texels in 4x4 tiles.
    for (unsigned y = 0; y < Size; ++y)
        for (unsigned x = 0; x < Size; ++x) {
            const size_t tile = (y / Tile) * (Size / Tile) + x / Tile;
            const size_t element = tile * Tile * Tile + (y % Tile) * Tile + x % Tile;
            const uint16_t color = (uint16_t{tiled[2 * element]} << 8) | tiled[2 * element + 1];
            const uint32_t r = color >> 11, g = (color >> 5) & 63, b = color & 31;
            rgba[y * Size + x] = 0xff000000u | ((b << 3 | b >> 2) << 16) |
                ((g << 2 | g >> 4) << 8) | (r << 3 | r >> 2);
        }
}
