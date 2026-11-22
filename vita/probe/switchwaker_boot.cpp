#include "disc.h"
#include "gpu.h"
#include <dolphin/dvd.h>
#include "pc_internal.h"
#include "cos_sdk/host_alloc.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

bool cos_asset_fill(void *array, unsigned int size, unsigned int id);

namespace {
SceUID log_fd = -1;
BootDisc *disc;
std::vector<uint8_t> dol;
struct AssetExit { int code; };

void log_format(const char *prefix, const char *format, va_list args)
{
    char message[384];
    std::vsnprintf(message, sizeof(message), format, args);
    probe_log(log_fd, "%s%s", prefix, message);
}
}

extern "C" s32 DVDConvertPathToEntrynum(const char *path)
{
    return disc && path ? boot_disc_file_index(disc, path) : -1;
}

extern "C" BOOL DVDFastOpen(s32 entry, DVDFileInfo *file)
{
    uint32_t offset, length;
    if (!disc || !file || boot_disc_file_at(disc, entry, &offset, &length) < 0)
        return 0;
    *file = {};
    file->startAddr = offset;
    file->length = length;
    return 1;
}

extern "C" BOOL DVDOpen(const char *path, DVDFileInfo *file)
{
    if (!DVDFastOpen(DVDConvertPathToEntrynum(path), file)) {
        probe_log(log_fd, "dvd: could not find %s\n", path ? path : "(null)");
        return 0;
    }
    probe_log(log_fd, "dvd: reading %s (%u bytes)\n", path, static_cast<unsigned int>(file->length));
    return 1;
}

extern "C" BOOL DVDClose(DVDFileInfo *)
{
    return 1;
}

extern "C" void DVDInit()
{
    // Vita mounts the game image explicitly before JFW calls DVDInit.
    probe_log(log_fd, "dvd: JFW initialization, disc %s\n", disc ? "mounted" : "unavailable");
}

extern "C" s32 DVDReadPrio(DVDFileInfo *file, void *output, s32 length, s32 offset, s32)
{
    if (!disc || !file || offset < 0 || length < 0 || (length && !output) ||
        uint64_t(offset) + uint64_t(length) > file->length)
        return -1;
    return boot_disc_read(disc, uint64_t(file->startAddr) + uint64_t(offset), output, size_t(length)) == 0
        ? length : -1;
}

extern "C" s32 DVDGetCommandBlockStatus(const DVDCommandBlock *block)
{
    return block ? block->state : DVD_STATE_END;
}

extern "C" BOOL DVDReadAsyncPrio(DVDFileInfo *file, void *output, s32 length, s32 offset,
                                 DVDCallback callback, s32 priority)
{
    if (!disc || !file || !output || length < 0 || offset < 0 ||
        uint64_t(offset) > file->length ||
        uint64_t(offset) + uint64_t(length) >= uint64_t(file->length) + 32)
        return FALSE;
    const u32 remaining = file->length - static_cast<u32>(offset);
    const s32 available = static_cast<s32>(std::min(remaining, static_cast<u32>(length)));
    const s32 read = DVDReadPrio(file, output, available, offset, priority);
    if (read >= 0 && available < length)
        std::memset(static_cast<uint8_t *>(output) + available, 0, length - available);
    file->cb.transferredSize = read < 0 ? 0 : static_cast<u32>(length);
    file->cb.state = read < 0 ? DVD_STATE_FATAL_ERROR : DVD_STATE_END;
    // The bootstrap completes inline. JKRDvdFile queues the callback result before waiting.
    if (callback)
        callback(read < 0 ? read : length, file);
    return TRUE;
}

int boot_mount_game_dvd(SceUID log, const char *path)
{
    log_fd = log;
    disc = boot_disc_open(log, path);
    return disc ? 0 : -1;
}

void boot_unmount_game_dvd()
{
    boot_disc_close(disc);
    disc = nullptr;
}

extern "C" const u8 *DVDGetDOLLocation(s32 *out_size)
{
    *out_size = 0;
    uint32_t offset, size;
    boot_disc_dol(disc, &offset, &size);
    if (size > static_cast<uint32_t>(std::numeric_limits<s32>::max()))
        return nullptr;
    if (dol.empty()) {
        dol.resize(size);
        if (boot_disc_read(disc, offset, dol.data(), dol.size()) < 0) {
            dol.clear();
            return nullptr;
        }
    }
    *out_size = static_cast<s32>(dol.size());
    return dol.data();
}

namespace pc {
Config gConfig;
bool probeStageArchive(const char *path, uint32_t *expandedSize);
bool probeExpandYaz0(const uint8_t *source, size_t size, uint8_t *output, size_t capacity);
bool probeTitleTexture(cos_sdk::HostVector<uint8_t>& texture);

void writef(int, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    log_format("", format, args);
    va_end(args);
}

void discMessage(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    log_format("boot: asset error: ", format, args);
    va_end(args);
    probe_log(log_fd, "\n");
}

[[noreturn]] void exitDisc()
{
    pc_exit(PC_EXIT_DISC);
}
}

extern "C" [[noreturn]] void pc_exit(int code)
{
    throw AssetExit{code};
}

int boot_switchwaker_assets(SceUID log, const char *path)
{
    log_fd = log;
    disc = boot_disc_open(log, path);
    if (!disc)
        return -1;
    probe_log(log, "boot: registering an asset array\n");
#include "assets/black_tex.h"
    probe_log(log, "boot: entering SwitchWaker pc::loadAssets\n");
    int status = 0;
    try {
        pc::loadAssets();
        probe_log(log, "boot: SwitchWaker asset loader returned\n");
    } catch (const AssetExit &failure) {
        status = failure.code == 0 ? -1 : failure.code;
        probe_log(log, "boot: asset loader exited with %d\n", failure.code);
    }
    std::vector<uint8_t>().swap(dol);
    boot_disc_close(disc);
    disc = nullptr;
    return status;
}

int boot_switchwaker_archives(SceUID log, const char *path)
{
    log_fd = log;
    disc = boot_disc_open(log, path);
    if (!disc)
        return -1;
    int fail = 0;
    for (const char *archive : {"/res/Stage/sea_T/Stage.arc", "/res/Stage/sea/LODALL.arc"}) {
        uint32_t size = 0;
        const bool ok = pc::probeStageArchive(archive, &size);
        probe_log(log, "check: %s %s (RARC: %u bytes)\n", archive, ok ? "PASS" : "FAIL",
                  static_cast<unsigned int>(size));
        fail |= !ok;
    }
    boot_disc_close(disc);
    disc = nullptr;
    return fail ? -1 : 0;
}

int boot_expand_reference(const uint8_t *source, size_t size, uint8_t *output, size_t capacity)
{
    return pc::probeExpandYaz0(source, size, output, capacity) ? 0 : -1;
}

int boot_read_title(SceUID log, const char *path, ProbeImage *image)
{
    log_fd = log;
    disc = boot_disc_open(log, path);
    if (!disc)
        return -1;
    cos_sdk::HostVector<uint8_t> texture;
    const bool found = pc::probeTitleTexture(texture);
    boot_disc_close(disc);
    disc = nullptr;
    if (!found) {
        probe_log(log, "title: logo_zelda_main.bti could not be loaded\n");
        return -1;
    }
    return probe_decode_bti(log, texture.data(), texture.size(), image);
}
