// Wind Waker's own small data files, read straight from the disc by the DVD
// thread and handed to the game as they are: converted to host byte order
// where d_s_logo.cpp hands them over (patch 0010). Layouts from the code
// that reads them.
#include <stdint.h>
#include <string.h>
#include <stdio.h>

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

// --- Stage and room files (.dzs, .dzr) ----------------------------------------
// s32 chunk count; per chunk {u32 tag, s32 count, u32 offset}
// (dStage_nodeHeader); at the offset, `count` records of the chunk's type
// (d_stage.h). Fields named from where the stage loaders read them.
namespace {
inline uint32_t be32v(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
inline void swn(uint8_t* p, uint32_t count, uint32_t width)
{
	for (uint32_t i = 0; i < count; i++)
		width == 4 ? sw32(p + i * 4) : sw16(p + i * 2);
}

// fopAcM_prmBase_class at +8 of an actor record: u32 parameters, cXyz,
// csXyz angle, u16 set ID.
void actor_rec(uint8_t* r)
{
	sw32(r + 0x08);
	swn(r + 0x0C, 3, 4);
	swn(r + 0x18, 4, 2);
}

struct ChunkType {
	char tag[5];
	uint32_t size; // record size
	void (*conv)(uint8_t* rec);
};

void c_actor(uint8_t* r) { actor_rec(r); }                     // 0x20: name, base
void c_tgsc(uint8_t* r) { actor_rec(r); }                      // 0x24: name, base, scale (u8 x 3)
void c_arrow(uint8_t* r) { swn(r, 3, 4); swn(r + 0x0C, 4, 2); } // cXyz, csXyz, s16
void c_fili(uint8_t* r) { swn(r, 2, 4); }                       // u32 param, f32 sea level
void c_pale(uint8_t* r) { swn(r + 0x24, 2, 4); }                // colours (bytes), fog z start/end
void c_colo(uint8_t* r) { sw32(r + 0x08); }                     // palette ids, change rate
void c_virt(uint8_t* r) { swn(r, 4, 4); }                       // 4 x u32, then colours
void c_light(uint8_t* r) { swn(r, 4, 4); }                      // position, radius, then bytes
void c_path(uint8_t* r) { swn(r, 2, 2); sw32(r + 0x08); }      // u16 num, u16 next, bytes, u32 points
void c_pnt(uint8_t* r) { swn(r + 0x04, 3, 4); }                 // 4 bytes, position
void c_sond(uint8_t* r) { swn(r + 0x08, 3, 4); }                // name, position, bytes
void c_flor(uint8_t* r) { sw32(r); }                            // f32, bytes
void c_mult(uint8_t* r) { swn(r, 2, 4); sw16(r + 0x08); }       // trans x/z, angle, bytes
void c_ship(uint8_t* r) { swn(r, 3, 4); sw16(r + 0x0C); }       // position, angle, bytes
void c_2dma(uint8_t* r) { swn(r, 13, 4); }                      // 13 floats, bytes
void c_dmap(uint8_t* r) { swn(r, 4, 4); }                       // 4 floats
void c_u32(uint8_t* r) { sw32(r); }                             // MEMA: u32 array
void c_stag(uint8_t* r) // stage_stag_info_class: the file holds its first 0x14 bytes
{
	swn(r, 2, 4);        // near, far
	sw16(r + 0x0A);      // particle scene number
	swn(r + 0x0C, 2, 4); // stage type and schbit, schbit enable and far plane
}

const ChunkType k_chunks[] = {
	{"STAG", 0x14, c_stag}, // (the struct is 0x20; 0x14 in the file)
	{"ACTR", 0x20, c_actor}, {"PLYR", 0x20, c_actor}, {"TGOB", 0x20, c_actor}, {"TRES", 0x20, c_actor},
	{"ACT0", 0x20, c_actor}, {"ACT1", 0x20, c_actor}, {"ACT2", 0x20, c_actor}, {"ACT3", 0x20, c_actor},
	{"ACT4", 0x20, c_actor}, {"ACT5", 0x20, c_actor}, {"ACT6", 0x20, c_actor}, {"ACT7", 0x20, c_actor},
	{"ACT8", 0x20, c_actor}, {"ACT9", 0x20, c_actor}, {"ACTa", 0x20, c_actor}, {"ACTb", 0x20, c_actor},
	{"TRE0", 0x20, c_actor}, {"TRE1", 0x20, c_actor}, {"TRE2", 0x20, c_actor}, {"TRE3", 0x20, c_actor},
	{"TRE4", 0x20, c_actor}, {"TRE5", 0x20, c_actor}, {"TRE6", 0x20, c_actor}, {"TRE7", 0x20, c_actor},
	{"TRE8", 0x20, c_actor}, {"TRE9", 0x20, c_actor}, {"TREa", 0x20, c_actor}, {"TREb", 0x20, c_actor},
	{"SCOB", 0x24, c_tgsc}, {"TGSC", 0x24, c_tgsc}, {"DOOR", 0x24, c_tgsc}, {"TGDR", 0x24, c_tgsc},
	{"SCO0", 0x24, c_tgsc}, {"SCO1", 0x24, c_tgsc}, {"SCO2", 0x24, c_tgsc}, {"SCO3", 0x24, c_tgsc},
	{"SCO4", 0x24, c_tgsc}, {"SCO5", 0x24, c_tgsc}, {"SCO6", 0x24, c_tgsc}, {"SCO7", 0x24, c_tgsc},
	{"SCO8", 0x24, c_tgsc}, {"SCO9", 0x24, c_tgsc}, {"SCOa", 0x24, c_tgsc}, {"SCOb", 0x24, c_tgsc},
	{"CAMR", 0x14, NULL}, {"RCAM", 0x14, NULL},
	{"AROB", 0x14, c_arrow}, {"RARO", 0x14, c_arrow},
	{"FILI", 0x08, c_fili},
	{"Pale", 0x2C, c_pale}, {"Colo", 0x0C, c_colo}, {"EnvR", 0x08, NULL}, {"Virt", 0x24, c_virt},
	{"LGHT", 0x1C, c_light}, {"LGTV", 0x1C, c_light},
	{"PATH", 0x0C, c_path}, {"RPAT", 0x0C, c_path}, {"PPNT", 0x10, c_pnt}, {"RPPN", 0x10, c_pnt},
	{"SOND", 0x1C, c_sond}, {"FLOR", 0x14, c_flor}, {"LBNK", 0x01, NULL}, {"MULT", 0x0C, c_mult},
	{"SCLS", 0x0C, NULL}, {"EVNT", 0x18, NULL}, {"SHIP", 0x10, c_ship},
	{"2DMA", 0x38, c_2dma}, {"2Dma", 0x38, c_2dma}, {"DMAP", 0x10, c_dmap},
	{"MECO", 0x02, NULL}, {"MEMA", 0x04, c_u32},
	{"RTBL", 0x04, NULL}, // offsets to room records: converted after the others
};

const ChunkType* find_chunk(uint32_t tag)
{
	for (unsigned i = 0; i < sizeof(k_chunks) / sizeof(k_chunks[0]); i++) {
		const char* t = k_chunks[i].tag;
		if (tag == (((uint32_t)(uint8_t)t[0] << 24) | ((uint32_t)(uint8_t)t[1] << 16) |
		            ((uint32_t)(uint8_t)t[2] << 8) | (uint8_t)t[3]))
			return &k_chunks[i];
	}
	return NULL;
}
} // namespace

extern "C" void port_log(const char* fmt, ...);

// Returns 1 if converted, 0 if not a stage file (or converted before).
extern "C" int tww_endian_stage(void* data, uint32_t size, const char* name)
{
	uint8_t* p = (uint8_t*)data;
	if (!p || size < 4)
		return 0;
	uint32_t n = be32v(p);
	// Converted before: the big-endian read of a host-order count is huge.
	if (n == 0 || n > 256 || 4 + n * 12 > size)
		return 0;
	sw32(p);
	for (uint32_t i = 0; i < n; i++) {
		uint8_t* h = p + 4 + i * 12;
		// The tag stays as its four characters: the loaders compare its
		// bytes with the chunk names' (dStage_dt_c_decode, actor_node_names).
		swn(h + 4, 2, 4);
		uint32_t tag = be32v(h), count = ld32(h + 4), off = ld32(h + 8);
		const ChunkType* t = find_chunk(tag);
		if (!t) {
			static int warned;
			if (warned++ < 16)
				port_log("[tww] %s: chunk '%c%c%c%c' (%u records) not converted\n", name ? name : "stage",
				         (char)(tag >> 24), (char)(tag >> 16), (char)(tag >> 8), (char)tag, count);
			continue;
		}
		if (tag == 0x5254424C /* RTBL */)
			continue; // handled below (offsets to records)
		// The room up to the next chunk in the file (or its end): a record
		// size wrong here would convert a neighbour's bytes.
		uint32_t next = size;
		for (uint32_t j = 0; j < n; j++) {
			uint32_t o = ld32(p + 4 + j * 12 + 8);
			if (j != i && o > off && o < next)
				next = o;
		}
		if (off && off < next && count && count > (next - off) / t->size) {
			port_log("[tww] %s: chunk %.4s: %u records of 0x%x bytes do not fit before the next chunk (0x%x bytes there): not converted\n",
			         name ? name : "stage", (const char*)h, count, t->size, next - off);
			continue;
		}
		if (!off || off > size || count > (size - off) / t->size) {
			port_log("[tww] %s: chunk %.4s (%u x %u bytes at 0x%x) outside the file (%u bytes): not converted\n",
			         name ? name : "stage", (const char*)h, count, t->size, off, size);
			continue;
		}
		if (!t->conv)
			continue;
		for (uint32_t k = 0; k < count; k++)
			t->conv(p + off + k * t->size);
	}
	// What the file holds (diagnostics; one line per file).
	{
		char line[512];
		int len = 0;
		for (uint32_t i = 0; i < n && len < (int)sizeof(line) - 16; i++) {
			const uint8_t* h = p + 4 + i * 12;
			len += snprintf(line + len, sizeof(line) - len, " %.4s:%u", (const char*)h, ld32(h + 4));
		}
		port_log("[tww] %s (%u bytes at %p): %u chunks:%s\n", name ? name : "stage", size, data, n, line);
	}
	// RTBL: u32 offsets to roomRead_data_class {u8 num, u8, u8, pad,
	// u32 room list offset}; several may share a record.
	for (uint32_t i = 0; i < n; i++) {
		uint8_t* h = p + 4 + i * 12;
		if (be32v(h) != 0x5254424C)
			continue;
		uint32_t count = ld32(h + 4), off = ld32(h + 8);
		if (!off || off > size || count > (size - off) / 4) {
			port_log("[tww] %s: RTBL (%u at 0x%x) outside the file (%u bytes): not converted\n",
			         name ? name : "stage", count, off, size);
			continue;
		}
		uint32_t done[64];
		uint32_t ndone = 0;
		for (uint32_t k = 0; k < count; k++) {
			sw32(p + off + k * 4);
			uint32_t rec = ld32(p + off + k * 4);
			bool seen = false;
			for (uint32_t d = 0; d < ndone; d++)
				seen |= done[d] == rec;
			if (seen || rec + 8 > size)
				continue;
			sw32(p + rec + 4);
			if (ndone < 64)
				done[ndone++] = rec;
		}
		for (uint32_t k = 0; k < count && k < 3; k++) {
			uint32_t rec = ld32(p + off + k * 4);
			port_log("[tww] %s: RTBL[%u] -> record 0x%x (rooms %u, list at 0x%x)\n", name ? name : "stage", k, rec,
			         rec + 8 <= size ? p[rec] : 0, rec + 8 <= size ? ld32(p + rec + 4) : 0);
		}
	}
	return 1;
}

// --- Event list (event_list.dat) --------------------------------------------------
// event_binary_data_header (0x40: 7 x {u32 offset, s32 count}), then
// events (0xB0), staff (0x50), cuts (0x50), data (0x40) records, f32 and
// s32 arrays, and strings (d_event_data.h).
extern "C" int tww_endian_event_list(void* data, uint32_t size)
{
	uint8_t* p = (uint8_t*)data;
	if (!p || size < 0x40)
		return 0;
	uint32_t top0 = be32v(p);
	if (top0 != 0x40) // the event records follow the header
		return 0;
	swn(p, 14, 4);
	struct { uint32_t off, num; } c[7];
	for (int i = 0; i < 7; i++) {
		c[i].off = ld32(p + i * 8);
		c[i].num = ld32(p + i * 8 + 4);
	}
	const uint32_t rec[4] = {0xB0, 0x50, 0x50, 0x40};
	for (int i = 0; i < 7; i++) {
		uint32_t w = i < 4 ? rec[i] : 4;
		if (!c[i].off || c[i].off > size || c[i].num > (size - c[i].off) / w)
			continue;
		for (uint32_t k = 0; k < c[i].num; k++) {
			uint8_t* r = p + c[i].off + k * w;
			switch (i) {
			case 0: // dEvDtEvent_c: name, 29 words, a byte at 0x94, 6 words
				swn(r + 0x20, 29, 4);
				swn(r + 0x98, 6, 4);
				break;
			case 1: // dEvDtStaff_c: name, 5 ints, 4 bytes, 2 words, 2 x s16, bytes
				swn(r + 0x20, 5, 4);
				swn(r + 0x38, 2, 4);
				swn(r + 0x40, 2, 2);
				break;
			case 2: // dEvDtCut_c: name, 12 words
				swn(r + 0x20, 12, 4);
				break;
			case 3: // dEvDtData_c: name, 8 words
				swn(r + 0x20, 8, 4);
				break;
			case 4: // FData: f32
			case 5: // IData: s32
				sw32(r);
				break;
			default: // SData: characters
				break;
			}
		}
	}
	return 1;
}

// Collision files (*.dzb, cBgD_t, c_bg_w.h): a 0x34-byte header of six
// {s32 count, u32 offset} pairs and a u32 flag, then the tables: vertices
// (3 f32), triangles (5 u16), blocks (u16), tree nodes (10 u16), groups
// (0x34: u32 name offset, 3 f32 scale, 3 s16 rotation + pad, 3 f32
// translation, 6 u16, u32 info), and triangle infos (4 u32). Offsets are
// from the file start; cBgS::ConvDzb turns them into pointers afterwards.
extern "C" int tww_endian_dzb(void* data, uint32_t size, const char* name)
{
	uint8_t* b = (uint8_t*)data;
	if (size < 0x34)
		return 0;
	// The first table usually follows the header; in host order this
	// offset would read as a huge number.
	uint32_t first = be32v(b + 0x04) ? be32v(b + 0x04) : be32v(b + 0x0C);
	if (first < 0x34 || first > size) {
		if (ld32(b + 0x04) >= 0x34 && ld32(b + 0x04) <= size)
			return 1; // already converted
		port_log("[tww] %s: not a collision file (first table at 0x%x)\n", name ? name : "dzb", first);
		return 0;
	}
	static const struct { uint32_t rec, width, n; } tbl[6] = {
		{12, 4, 3}, {10, 2, 5}, {2, 2, 1}, {20, 2, 10}, {0x34, 0, 0}, {16, 4, 4}};
	swn(b, 13, 4);
	for (int t = 0; t < 6; t++) {
		uint32_t cnt = ld32(b + t * 8), off = ld32(b + t * 8 + 4);
		if (cnt == 0 || off == 0)
			continue;
		if (off >= size || (uint64_t)cnt * tbl[t].rec > size - off) {
			port_log("[tww] %s: table %d (%u x 0x%x at 0x%x) runs past the end (0x%x): not converted\n",
			         name ? name : "dzb", t, cnt, tbl[t].rec, off, size);
			continue;
		}
		for (uint32_t i = 0; i < cnt; i++) {
			uint8_t* r = b + off + i * tbl[t].rec;
			if (t == 4) { // group
				swn(r, 4, 4);         // name, scale
				swn(r + 0x10, 3, 2);  // rotation
				swn(r + 0x18, 3, 4);  // translation
				swn(r + 0x24, 6, 2);
				sw32(r + 0x30);
			} else {
				swn(r, tbl[t].n, tbl[t].width);
			}
		}
	}
	return 1;
}

// ---- JStudio cutscene scripts (*.stb) -------------------------------------
// The structure is converted here; STB object paragraphs' payloads, whose
// meaning only their readers know, are read big-endian where they are used
// (patch 0029). The embedded function values (a JFVB block) are converted
// whole: their paragraph contents are 4-byte words except an ID-string list
// (0x10) and a pair of u16s (0x15).
namespace {

inline uint16_t ld16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }
inline uint16_t sr16(uint8_t* p) { sw16(p); return ld16(p); } // swap, then the value
inline uint32_t sr32(uint8_t* p) { sw32(p); return ld32(p); }

// A paragraph header (JGadget::binary::parseVariableUInt_16_32_following):
// u16 size, u16 type; or, with the top bit set, a 31-bit size in two u16s
// and a u32 type. Returns the content's offset, size and type.
bool stb_paragraph(uint8_t* b, uint32_t off, uint32_t end, uint32_t* content, uint32_t* size, uint32_t* type)
{
	if (off + 4 > end)
		return false;
	uint16_t first = (uint16_t)(b[off] << 8 | b[off + 1]);
	if (!(first & 0x8000)) {
		sw16(b + off);
		sw16(b + off + 2);
		*size = ld16(b + off);
		*type = ld16(b + off + 2);
		*content = off + 4;
	} else {
		if (off + 8 > end)
			return false;
		sw16(b + off);
		sw16(b + off + 2);
		sw32(b + off + 4);
		*size = ((uint32_t)(ld16(b + off) & 0x7FFF) << 16) | ld16(b + off + 2);
		*type = ld32(b + off + 4);
		*content = off + 8;
	}
	return *content + *size <= end;
}

void fvb_convert(uint8_t* b, uint32_t start, uint32_t end)
{
	if (start + 0x10 > end || memcmp(b + start, "FVB", 3) != 0)
		return;
	sw16(b + start + 4);
	sw16(b + start + 6);
	sw32(b + start + 8);
	uint32_t nblocks = sr32(b + start + 0x0C);
	uint32_t pos = start + 0x10;
	for (uint32_t i = 0; i < nblocks && pos + 8 <= end; i++) {
		uint32_t bsize = sr32(b + pos);
		sw16(b + pos + 4); // type
		uint16_t idsize = sr16(b + pos + 6);
		if (bsize < 8 || pos + bsize > end)
			break;
		uint32_t bend = pos + bsize;
		uint32_t p = pos + 8 + ((idsize + 3) & ~3u);
		while (p < bend) {
			uint32_t c, size, type;
			if (!stb_paragraph(b, p, bend, &c, &size, &type) || type == 0)
				break;
			if (type == 0x10) { // u32 count, then (u32 size, ID bytes) entries
				uint32_t n = sr32(b + c), q = c + 4;
				for (uint32_t k = 0; k < n && q + 4 <= c + size; k++) {
					uint32_t len = sr32(b + q);
					q += 4 + ((len + 3) & ~3u);
				}
			} else if (type == 0x15) {
				swn(b + c, size / 2, 2);
			} else {
				swn(b + c, size / 4, 4);
			}
			p = c + ((size + 3) & ~3u);
		}
		pos = bend;
	}
}

} // namespace

extern "C" int tww_endian_stb(void* data, uint32_t size, const char* name)
{
	uint8_t* b = (uint8_t*)data;
	if (size < 0x20 || memcmp(b, "STB", 3) != 0)
		return 0;
	if (b[4] == 0xFF && b[5] == 0xFE)
		return 1; // already converted (the byte-order mark reads 0xFEFF natively)
	sw16(b + 4);  // byte order (0xFEFF)
	sw16(b + 6);  // version
	sw32(b + 8);
	uint32_t nblocks = sr32(b + 0x0C);
	swn(b + 0x18, 4, 2); // target: u16[3], version
	uint32_t pos = 0x20;
	for (uint32_t i = 0; i < nblocks && pos + 8 <= size; i++) {
		uint32_t bsize = sr32(b + pos);
		uint32_t type = sr32(b + pos + 4);
		if (bsize < 8 || pos + bsize > size) {
			port_log("[tww] %s: block %u (0x%x bytes at 0x%x) runs past the end\n", name, i, bsize, pos);
			break;
		}
		uint32_t bend = pos + bsize;
		if (type == 'JFVB') {
			fvb_convert(b, pos + 8, bend);
		} else if (pos + 0x0C <= bend) {
			sw16(b + pos + 8); // flag
			uint16_t idsize = sr16(b + pos + 0x0A);
			uint32_t p = pos + 0x0C + ((idsize + 3) & ~3u);
			// The object's sequence: u32 words, type in the top byte; types
			// from 0x80 carry `param` bytes of paragraphs.
			while (p + 4 <= bend) {
				uint32_t head = sr32(b + p);
				uint32_t t = head >> 24, param = head & 0xFFFFFF;
				p += 4;
				if (t >= 0x80) {
					uint32_t pend = p + param;
					if (pend > bend)
						break;
					uint32_t q = p;
					while (q < pend) {
						uint32_t c, psize, ptype;
						if (!stb_paragraph(b, q, pend, &c, &psize, &ptype))
							break;
						q = c + ((psize + 3) & ~3u);
					}
					p = pend;
				}
			}
		}
		pos = bend;
	}
	return 1;
}
