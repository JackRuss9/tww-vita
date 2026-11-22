#include "gx_probe.h"
#include "aurora_fifo.hpp"

#include <array>
#include <span>

namespace {
constexpr uint8_t LoadCp = 0x08, Quads = 0x80, Triangles = 0x90;
constexpr uint8_t VcdLow = 0x50, VcdHigh = 0x60, VatA = 0x70;
constexpr uint32_t DirectPositionAndColor = (1u << 9) | (1u << 13);
constexpr uint32_t XyzFloatRgba8 = 1u | (4u << 1) | (1u << 13) | (5u << 14);
constexpr size_t VertexBytes = 16;
constexpr uint32_t Background = 0xff302018;
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

void vertex(float x, float y, uint32_t rgba)
{
    fifo::write_f32(x);
    fifo::write_f32(y);
    fifo::write_f32(0.5f);
    fifo::write_u8(static_cast<uint8_t>(rgba));
    fifo::write_u8(static_cast<uint8_t>(rgba >> 8));
    fifo::write_u8(static_cast<uint8_t>(rgba >> 16));
    fifo::write_u8(static_cast<uint8_t>(rgba >> 24));
}

void record_scene()
{
    cp_register(VcdLow, DirectPositionAndColor);
    cp_register(VcdHigh, 0);
    cp_register(VatA, XyzFloatRgba8);
    fifo::write_u8(Quads); // GX_VTXFMT0
    fifo::write_u16(4);
    vertex(-1.0f, -1.0f, Background);
    vertex( 1.0f, -1.0f, Background);
    vertex( 1.0f,  1.0f, Background);
    vertex(-1.0f,  1.0f, Background);
    fifo::write_u8(Triangles);
    fifo::write_u16(3);
    vertex(-0.72f,  0.65f, 0xff30eaff);
    vertex( 0.72f,  0.65f, 0xffefff30);
    vertex( 0.0f,  -0.76f, 0xffe830ff);
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
            vcd_low != DirectPositionAndColor || vcd_high != 0 || vat_a != XyzFloatRgba8)
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
                aurora::bswap(load_be32(data + 12)),
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
