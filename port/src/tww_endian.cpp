// Wind Waker's own small data files, read straight from the disc by the DVD
// thread and handed to the game as they are: converted to host byte order
// where d_s_logo.cpp hands them over (patch 0010). Layouts from the code
// that reads them.
#include <stdint.h>
#include <string.h>

namespace {
inline void sw16(uint8_t* p) { uint8_t t = p[0]; p[0] = p[1]; p[1] = t; }
inline void sw32(uint8_t* p)
{
	uint8_t t = p[0]; p[0] = p[3]; p[3] = t;
	t = p[1]; p[1] = p[2]; p[2] = t;
}
inline uint32_t ld32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
} // namespace

// /res/ItemTable/item_table.bin (ItemTableList): "ITEM_TABLE", s16 entry
// count at 0x0A, u32 at 0x0C, then bytes.
extern "C" void tww_endian_item_table(void* data, uint32_t size)
{
	uint8_t* p = (uint8_t*)data;
	if (!p || size < 0x10 || memcmp(p, "ITEM_TAB", 8) != 0)
		return;
	sw16(p + 0x0A);
	sw32(p + 0x0C);
}

// Actor data (dADM::SetData): s32 block count; per block {u32 tag, u32 count,
// u32 offset}; ACFN and ACNA are arrays of u32 string offsets, ACDS bytes.
extern "C" void tww_endian_actor_data(void* data, uint32_t size)
{
	uint8_t* p = (uint8_t*)data;
	if (!p || size < 4)
		return;
	sw32(p);
	uint32_t n = ld32(p);
	if (n > 64 || 4 + n * 12 > size)
		return;
	for (uint32_t i = 0; i < n; i++) {
		uint8_t* h = p + 4 + i * 12;
		sw32(h);
		sw32(h + 4);
		sw32(h + 8);
		uint32_t tag = ld32(h), count = ld32(h + 4), off = ld32(h + 8);
		if ((tag == 0x4143464E /* ACFN */ || tag == 0x41434E41 /* ACNA */) &&
		    off <= size && count <= (size - off) / 4) {
			for (uint32_t k = 0; k < count; k++)
				sw32(p + off + k * 4);
		}
	}
}

// Field-map check points (dMap_FmapChkPnt): u32 last index, then 8-byte
// FmapChkPnt {char, char, s16, s16, u8, pad} to the end of the file.
extern "C" void tww_endian_fmap_data(void* data, uint32_t size)
{
	uint8_t* p = (uint8_t*)data;
	if (!p || size < 4)
		return;
	sw32(p);
	for (uint32_t off = 4; off + 8 <= size; off += 8) {
		sw16(p + off + 2);
		sw16(p + off + 4);
	}
}
