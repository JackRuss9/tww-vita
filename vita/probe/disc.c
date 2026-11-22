#include "disc.h"
#include "gpu.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <psp2/io/fcntl.h>

enum { CISO_HEADER_SIZE = 0x8000, CISO_MAP_SIZE = CISO_HEADER_SIZE - 8, FST_ENTRY_SIZE = 12 };

struct BootDisc {
    SceUID fd;
    uint32_t block_size;
    uint16_t map[CISO_MAP_SIZE];
    uint8_t *table;
    uint32_t table_size;
    uint32_t table_entries;
    uint32_t dol_offset;
    uint32_t dol_size;
};

static uint32_t be32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static int read_at(SceUID fd, uint64_t offset, void *output, size_t bytes)
{
    uint8_t *out = output;
    while (bytes > 0) {
        size_t request = bytes < 128 * 1024 ? bytes : 128 * 1024;
        int got = sceIoPread(fd, out, request, (SceOff)offset);
        if (got <= 0)
            return -1;
        out += got;
        offset += (size_t)got;
        bytes -= (size_t)got;
    }
    return 0;
}

static int load_map(BootDisc *disc, SceUID log)
{
    uint8_t *header = malloc(CISO_HEADER_SIZE);
    if (!header)
        return -1;
    int result = -1;
    if (read_at(disc->fd, 0, header, CISO_HEADER_SIZE) < 0 || memcmp(header, "CISO", 4) != 0)
        goto done;

    disc->block_size = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
        ((uint32_t)header[6] << 16) | ((uint32_t)header[7] << 24);
    if (disc->block_size == 0 || header[8] != 1)
        goto done;

    unsigned int stored = 0;
    for (size_t i = 0; i < CISO_MAP_SIZE; ++i)
        disc->map[i] = header[8 + i] == 1 ? (uint16_t)stored++ : UINT16_MAX;

    SceOff size = sceIoLseek(disc->fd, 0, SCE_SEEK_END);
    uint64_t minimum = CISO_HEADER_SIZE + (uint64_t)stored * disc->block_size;
    if (size < 0 || (uint64_t)size < minimum)
        goto done;
    probe_log(log, "boot: CISO mapped %u blocks of %u bytes, file size %lld\n",
              stored, (unsigned int)disc->block_size, (long long)size);
    result = 0;
done:
    free(header);
    return result;
}

uint64_t boot_disc_addressable_bytes(const BootDisc *disc)
{
    return (uint64_t)CISO_MAP_SIZE * disc->block_size;
}

int boot_disc_read(const BootDisc *disc, uint64_t offset, void *output, size_t bytes)
{
    uint64_t limit = boot_disc_addressable_bytes(disc);
    if (offset > limit || bytes > limit - offset)
        return -1;
    uint8_t *out = output;
    while (bytes > 0) {
        uint32_t in_block = (uint32_t)(offset % disc->block_size);
        uint16_t physical = disc->map[offset / disc->block_size];
        size_t chunk = disc->block_size - in_block;
        if (chunk > bytes)
            chunk = bytes;
        if (physical == UINT16_MAX) {
            memset(out, 0, chunk);
        } else {
            uint64_t file_offset = CISO_HEADER_SIZE + (uint64_t)physical * disc->block_size + in_block;
            if (read_at(disc->fd, file_offset, out, chunk) < 0)
                return -1;
        }
        out += chunk;
        offset += chunk;
        bytes -= chunk;
    }
    return 0;
}

typedef struct {
    const uint8_t *bytes;
    size_t size;
    uint32_t count;
} Fst;

static const uint8_t *fst_find(const Fst *fst, const char *path)
{
    uint32_t end = fst->count;
    const size_t names = (size_t)fst->count * FST_ENTRY_SIZE;
    uint32_t i = 1;
    while (i < end) {
        const char *slash = strchr(path, '/');
        size_t wanted = slash ? (size_t)(slash - path) : strlen(path);
        const uint8_t *entry = fst->bytes + i * FST_ENTRY_SIZE;
        uint32_t next = entry[0] == 1 ? be32(entry + 8) : i + 1;
        size_t name = names + (be32(entry) & 0x00ffffffu);
        if (next <= i || next > end || name >= fst->size)
            return NULL;
        const char *start = (const char *)fst->bytes + name;
        const char *term = memchr(start, '\0', fst->size - name);
        if (!term)
            return NULL;
        if ((size_t)(term - start) == wanted && strncasecmp(path, start, wanted) == 0) {
            if (!slash)
                return entry[0] == 0 ? entry : NULL;
            if (entry[0] != 1)
                return NULL;
            path = slash + 1;
            end = next;
            ++i;
        } else {
            i = next;
        }
    }
    return NULL;
}

int boot_disc_file_index(const BootDisc *disc, const char *path)
{
    if (*path == '/')
        ++path;
    const Fst fst = {disc->table, disc->table_size, disc->table_entries};
    const uint8_t *entry = fst_find(&fst, path);
    return entry ? (int)((entry - fst.bytes) / FST_ENTRY_SIZE) : -1;
}

int boot_disc_file_at(const BootDisc *disc, int index, uint32_t *offset, uint32_t *size)
{
    if (index <= 0 || (uint32_t)index >= disc->table_entries)
        return -1;
    const uint8_t *entry = disc->table + (size_t)index * FST_ENTRY_SIZE;
    if (entry[0] != 0)
        return -1;
    *offset = be32(entry + 4);
    *size = be32(entry + 8);
    return 0;
}

int boot_disc_file(const BootDisc *disc, const char *path, uint32_t *offset, uint32_t *size)
{
    return boot_disc_file_at(disc, boot_disc_file_index(disc, path), offset, size);
}

int boot_read_banner(SceUID log, const char *path, uint32_t *pixels)
{
    BootDisc *disc = boot_disc_open(log, path);
    if (!disc)
        return -1;
    uint8_t banner[0x20 + BOOT_BANNER_WIDTH * BOOT_BANNER_HEIGHT * 2];
    uint32_t offset, size;
    int ok = boot_disc_file(disc, "/opening.bnr", &offset, &size) == 0 &&
        size >= sizeof(banner) && boot_disc_read(disc, offset, banner, sizeof(banner)) == 0 &&
        (memcmp(banner, "BNR1", 4) == 0 || memcmp(banner, "BNR2", 4) == 0);
    boot_disc_close(disc);
    if (!ok) {
        probe_log(log, "disc: opening.bnr missing or has an invalid header\n");
        return -1;
    }
    probe_log(log, "disc: opening.bnr %.4s (%u bytes)\n", (const char *)banner, (unsigned int)size);

    // GameCube banners use RGB5A3 texels in 4x4 tiles.
    const uint8_t *src = banner + 0x20;
    for (unsigned int y = 0; y < BOOT_BANNER_HEIGHT; ++y) {
        for (unsigned int x = 0; x < BOOT_BANNER_WIDTH; ++x) {
            unsigned int tile = (y / 4) * (BOOT_BANNER_WIDTH / 4) + x / 4;
            unsigned int i = (tile * 16 + (y % 4) * 4 + x % 4) * 2;
            unsigned int value = (src[i] << 8) | src[i + 1];
            unsigned int r, g, b;
            if (value & 0x8000) {
                r = ((value >> 10) & 31) * 255 / 31;
                g = ((value >> 5) & 31) * 255 / 31;
                b = (value & 31) * 255 / 31;
            } else {
                unsigned int alpha = (value >> 12) & 7;
                r = ((value >> 8) & 15) * 17 * alpha / 7;
                g = ((value >> 4) & 15) * 17 * alpha / 7;
                b = (value & 15) * 17 * alpha / 7;
            }
            pixels[y * BOOT_BANNER_WIDTH + x] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
    return 0;
}

void boot_disc_dol(const BootDisc *disc, uint32_t *offset, uint32_t *size)
{
    *offset = disc->dol_offset;
    *size = disc->dol_size;
}

void boot_disc_close(BootDisc *disc)
{
    if (disc->fd >= 0)
        sceIoClose(disc->fd);
    free(disc->table);
    free(disc);
}

BootDisc *boot_disc_open(SceUID log, const char *path)
{
    probe_log(log, "boot: opening %s\n", path);
    BootDisc *disc = calloc(1, sizeof(*disc));
    if (!disc) {
        probe_log(log, "boot: CISO mapping allocation failed\n");
        return NULL;
    }
    disc->fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (disc->fd < 0) {
        probe_log(log, "boot: open failed 0x%08x\n", (unsigned int)disc->fd);
        goto cleanup;
    }
    if (load_map(disc, log) < 0) {
        probe_log(log, "boot: CISO block map invalid or truncated\n");
        goto cleanup;
    }

    uint8_t boot[0x440];
    if (boot_disc_read(disc, 0, boot, sizeof(boot)) < 0 || be32(boot + 0x1c) != 0xc2339f3d ||
        memcmp(boot, "GZLE01", 6) != 0 || boot[7] != 0) {
        probe_log(log, "boot: expected Wind Waker USA revision 0\n");
        goto cleanup;
    }
    probe_log(log, "boot: GZLE01 revision 0\n");
    uint32_t dol_offset = be32(boot + 0x420);
    uint8_t dol_header[0x100];
    if (dol_offset < sizeof(boot) || boot_disc_read(disc, dol_offset, dol_header, sizeof(dol_header)) < 0 ||
        be32(dol_header + 0xe0) == 0) {
        probe_log(log, "boot: main.dol header invalid\n");
        goto cleanup;
    }
    uint64_t dol_size = sizeof(dol_header);
    // Match nod's raw DOL length, which upstream verifies by SHA-1.
    for (unsigned int i = 0; i < 18; ++i) {
        uint64_t section_end = (uint64_t)be32(dol_header + 4 * i) + be32(dol_header + 0x90 + 4 * i);
        if (section_end > dol_size)
            dol_size = section_end;
    }

    uint32_t fst_offset = be32(boot + 0x424), fst_size = be32(boot + 0x428);
    if (dol_size <= sizeof(dol_header) || (uint64_t)dol_offset + dol_size > fst_offset) {
        probe_log(log, "boot: main.dol sections out of range\n");
        goto cleanup;
    }
    disc->dol_offset = dol_offset;
    disc->dol_size = (uint32_t)dol_size;
    probe_log(log, "boot: main.dol at 0x%08x, %u bytes, entry 0x%08x\n",
        (unsigned int)dol_offset, (unsigned int)disc->dol_size, (unsigned int)be32(dol_header + 0xe0));

    uint8_t root[FST_ENTRY_SIZE];
    if (fst_offset < sizeof(boot) || fst_size < sizeof(root) ||
        boot_disc_read(disc, fst_offset, root, sizeof(root)) < 0 || root[0] != 1) {
        probe_log(log, "boot: FST root invalid\n");
        goto cleanup;
    }
    uint32_t count = be32(root + 8);
    if (count == 0 || count > fst_size / FST_ENTRY_SIZE) {
        probe_log(log, "boot: FST entry count invalid\n");
        goto cleanup;
    }
    disc->table = malloc(fst_size);
    if (!disc->table || boot_disc_read(disc, fst_offset, disc->table, fst_size) < 0) {
        probe_log(log, "boot: could not load FST (%u bytes)\n", (unsigned int)fst_size);
        goto cleanup;
    }
    disc->table_size = fst_size;
    disc->table_entries = count;
    probe_log(log, "boot: FST has %u entries\n", (unsigned int)count);
    return disc;

cleanup:
    boot_disc_close(disc);
    return NULL;
}
