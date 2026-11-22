#include "disc.h"
#include "gpu.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/fcntl.h>

enum { CISO_HEADER_SIZE = 0x8000, CISO_MAP_SIZE = CISO_HEADER_SIZE - 8, FST_ENTRY_SIZE = 12 };

typedef struct {
    SceUID fd;
    uint32_t block_size;
    uint16_t map[CISO_MAP_SIZE];
} Ciso;

static uint32_t be32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static int read_at(SceUID fd, uint64_t offset, void *output, size_t bytes)
{
    SceOff position = sceIoLseek(fd, (SceOff)offset, SCE_SEEK_SET);
    if (position < 0 || (uint64_t)position != offset)
        return -1;
    uint8_t *out = output;
    while (bytes > 0) {
        size_t request = bytes < 128 * 1024 ? bytes : 128 * 1024;
        int got = sceIoRead(fd, out, request);
        if (got <= 0)
            return -1;
        out += got;
        bytes -= (size_t)got;
    }
    return 0;
}

static int load_map(Ciso *disc, SceUID log)
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

static int disc_read(const Ciso *disc, uint64_t offset, void *output, size_t bytes)
{
    uint64_t limit = (uint64_t)CISO_MAP_SIZE * disc->block_size;
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
        if ((size_t)(term - start) == wanted && memcmp(path, start, wanted) == 0) {
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

static int probe_archive(const Ciso *disc, const Fst *fst, SceUID log, const char *path)
{
    probe_log(log, "boot: reading /%s\n", path);
    const uint8_t *entry = fst_find(fst, path);
    if (!entry)
        return -1;
    uint32_t offset = be32(entry + 4), size = be32(entry + 8);
    uint8_t prefix[16], tail[16];
    if (size < sizeof(prefix) || disc_read(disc, offset, prefix, sizeof(prefix)) < 0 ||
        disc_read(disc, (uint64_t)offset + size - sizeof(tail), tail, sizeof(tail)) < 0)
        return -1;
    if (memcmp(prefix, "Yaz0", 4) != 0 && memcmp(prefix, "RARC", 4) != 0)
        return -1;
    probe_log(log, "boot: archive %.4s at 0x%08x, %u bytes\n", (const char *)prefix,
        (unsigned int)offset, (unsigned int)size);
    return 0;
}

int probe_boot_disc(SceUID log, const char *path)
{
    probe_log(log, "boot: opening %s\n", path);
    Ciso *disc = malloc(sizeof(*disc));
    uint8_t *table = NULL;
    int result = -1;
    if (!disc) {
        probe_log(log, "boot: CISO mapping allocation failed\n");
        return -1;
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
    if (disc_read(disc, 0, boot, sizeof(boot)) < 0 || be32(boot + 0x1c) != 0xc2339f3d ||
        memcmp(boot, "GZLE01", 6) != 0 || boot[7] != 0) {
        probe_log(log, "boot: expected Wind Waker USA revision 0\n");
        goto cleanup;
    }
    probe_log(log, "boot: GZLE01 revision 0\n");
    uint32_t dol_offset = be32(boot + 0x420);
    uint8_t dol_header[0x100];
    if (dol_offset < sizeof(boot) || disc_read(disc, dol_offset, dol_header, sizeof(dol_header)) < 0 ||
        be32(dol_header + 0xe0) == 0) {
        probe_log(log, "boot: main.dol header invalid\n");
        goto cleanup;
    }
    probe_log(log, "boot: main.dol header at 0x%08x, entry 0x%08x\n",
        (unsigned int)dol_offset, (unsigned int)be32(dol_header + 0xe0));

    uint32_t fst_offset = be32(boot + 0x424), fst_size = be32(boot + 0x428);
    uint8_t root[FST_ENTRY_SIZE];
    if (fst_offset < sizeof(boot) || fst_size < sizeof(root) ||
        disc_read(disc, fst_offset, root, sizeof(root)) < 0 || root[0] != 1) {
        probe_log(log, "boot: FST root invalid\n");
        goto cleanup;
    }
    uint32_t count = be32(root + 8);
    if (count == 0 || count > fst_size / FST_ENTRY_SIZE) {
        probe_log(log, "boot: FST entry count invalid\n");
        goto cleanup;
    }
    table = malloc(fst_size);
    if (!table || disc_read(disc, fst_offset, table, fst_size) < 0) {
        probe_log(log, "boot: could not load FST (%u bytes)\n", (unsigned int)fst_size);
        goto cleanup;
    }
    Fst fst = {table, fst_size, count};
    probe_log(log, "boot: FST has %u entries\n", (unsigned int)count);
    if (probe_archive(disc, &fst, log, "res/Stage/sea_T/Stage.arc") < 0 ||
        probe_archive(disc, &fst, log, "res/Stage/sea/LODALL.arc") < 0) {
        probe_log(log, "boot: required archive could not be read\n");
        goto cleanup;
    }
    result = 0;

cleanup:
    if (disc->fd >= 0)
        sceIoClose(disc->fd);
    free(table);
    free(disc);
    return result;
}
