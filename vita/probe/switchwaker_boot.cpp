#include "disc.h"
#include "gpu.h"
#include <dolphin/dvd.h>
#include "pc_internal.h"

#include <cstdarg>
#include <cstdio>
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

// The upstream asset loader uses this synchronous subset of the DVD API.
extern "C" BOOL DVDOpen(const char *path, DVDFileInfo *file)
{
    uint32_t offset, length;
    if (boot_disc_file(disc, path, &offset, &length) < 0) {
        probe_log(log_fd, "dvd: could not find %s\n", path);
        return 0;
    }
    *file = {};
    file->startAddr = offset;
    file->length = length;
    probe_log(log_fd, "dvd: reading %s (%u bytes)\n", path, static_cast<unsigned int>(length));
    return 1;
}

extern "C" BOOL DVDClose(DVDFileInfo *)
{
    return 1;
}

extern "C" s32 DVDReadPrio(DVDFileInfo *file, void *output, s32 length, s32 offset, s32)
{
    if (offset < 0 || length < 0 || uint64_t(offset) + uint64_t(length) > file->length)
        return -1;
    return boot_disc_read(disc, uint64_t(file->startAddr) + uint64_t(offset), output, size_t(length)) == 0
        ? length : -1;
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
