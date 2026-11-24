#include <dolphin/card.h>
#include "pc/pc_harness.h"

#include <psp2/io/fcntl.h>
#include <cstddef>
#include <cstring>

namespace {
constexpr s32 SaveSize = 0x18000;
constexpr s32 SectorSize = 0x2000;
constexpr u32 Magic = 0x31435754; // TWC1
constexpr const char* Paths[] = {"ux0:data/tww/gczelda.card", "ux0:data/tww/gczelda.bak"};
constexpr SceUID NoEntry = static_cast<SceUID>(0x80010002u);

struct Snapshot {
    u32 magic;
    u32 generation;
    u32 occupied;
    CARDStat stat;
    u8 data[SaveSize];
    u32 checksum;
};

// SwitchWaker serializes CARD operations on its memory-card worker.
Snapshot card{}, candidate{};
int active = -1;
bool mounted, dirty;
s32 mountResult = CARD_RESULT_READY;
char game[4], maker[2];

u32 checksum(const Snapshot& value)
{
    const auto* bytes = reinterpret_cast<const u8*>(&value);
    u32 hash = 2166136261u;
    for (size_t i = 0; i < offsetof(Snapshot, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

bool transfer(SceUID fd, void* buffer, s32 size, bool write)
{
    auto* bytes = static_cast<u8*>(buffer);
    while (size > 0) {
        const int count = write ? sceIoWrite(fd, bytes, size) : sceIoRead(fd, bytes, size);
        if (count <= 0)
            return false;
        bytes += count;
        size -= count;
    }
    return true;
}

s32 read_snapshot(int slot)
{
    const SceUID fd = sceIoOpen(Paths[slot], SCE_O_RDONLY, 0);
    if (fd < 0)
        return fd == NoEntry ? CARD_RESULT_NOFILE : CARD_RESULT_IOERROR;
    const bool valid = sceIoLseek(fd, 0, SCE_SEEK_END) == sizeof(candidate)
        && sceIoLseek(fd, 0, SCE_SEEK_SET) == 0
        && transfer(fd, &candidate, sizeof(candidate), false)
        && candidate.magic == Magic && candidate.occupied <= 1
        && candidate.checksum == checksum(candidate);
    sceIoClose(fd);
    return valid ? CARD_RESULT_READY : CARD_RESULT_BROKEN;
}

s32 commit()
{
    if (!dirty)
        return CARD_RESULT_READY;
    // Write the other snapshot. A partial write leaves the current one intact.
    const int slot = active == 0 ? 1 : 0;
    card.magic = Magic;
    ++card.generation;
    card.checksum = checksum(card);
    const SceUID fd = sceIoOpen(Paths[slot], SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd >= 0) {
        const bool written = transfer(fd, &card, sizeof(card), true) && sceIoSyncByFd(fd, 0) >= 0;
        const int closed = sceIoClose(fd);
        if (written && closed >= 0) {
            active = slot;
            dirty = false;
            mountResult = CARD_RESULT_READY;
            pc_milestone("vita-card-saved");
            return CARD_RESULT_READY;
        }
    }
    --card.generation;
    pc_milestone("vita-card-write-failed");
    return CARD_RESULT_IOERROR;
}

s32 ready(s32 slot)
{
    if (slot != 0)
        return CARD_RESULT_NOCARD;
    return mounted ? mountResult : CARD_RESULT_NOCARD;
}

s32 file_ready(s32 slot, s32 number)
{
    const s32 result = ready(slot);
    if (result != CARD_RESULT_READY)
        return result;
    return card.occupied && number == 0 ? CARD_RESULT_READY : CARD_RESULT_NOFILE;
}

s32 range(const CARDFileInfo* file, s32 size, s32 offset)
{
    if (!file || offset < 0 || size < 0 || offset > SaveSize || size > SaveSize - offset)
        return CARD_RESULT_FATAL_ERROR;
    return file_ready(file->chan, file->fileNo);
}
}

extern "C" {
void CARDInit(const char* gameCode, const char* makerCode)
{
    std::memcpy(game, gameCode, sizeof(game));
    std::memcpy(maker, makerCode, sizeof(maker));
}

int CARDProbe(s32 slot) { return slot == 0; }

s32 CARDProbeEx(s32 slot, s32* size, s32* sector)
{
    if (slot != 0)
        return CARD_RESULT_NOCARD;
    if (size)
        *size = 4;
    if (sector)
        *sector = SectorSize;
    return CARD_RESULT_READY;
}

s32 CARDMount(s32 slot, void*, CARDCallback)
{
    if (slot != 0)
        return CARD_RESULT_NOCARD;
    if (mounted)
        return mountResult;
    active = -1;
    std::memset(&card, 0, sizeof(card));
    dirty = false;
    const s32 first = read_snapshot(0);
    if (first == CARD_RESULT_READY) {
        card = candidate;
        active = 0;
    }
    const s32 second = read_snapshot(1);
    if (second == CARD_RESULT_READY && (active < 0 || candidate.generation > card.generation)) {
        card = candidate;
        active = 1;
    }
    mounted = true;
    if (active >= 0 || (first == CARD_RESULT_NOFILE && second == CARD_RESULT_NOFILE))
        mountResult = CARD_RESULT_READY;
    else if (first == CARD_RESULT_IOERROR || second == CARD_RESULT_IOERROR)
        mountResult = CARD_RESULT_IOERROR;
    else
        mountResult = CARD_RESULT_BROKEN;
    if (card.occupied)
        pc_milestone("vita-card-loaded");
    else if (mountResult == CARD_RESULT_READY)
        pc_milestone("vita-card-empty");
    return mountResult;
}

s32 CARDUnmount(s32 slot)
{
    if (slot != 0)
        return CARD_RESULT_NOCARD;
    mounted = false;
    return CARD_RESULT_READY;
}

s32 CARDCheck(s32 slot) { return ready(slot); }

s32 CARDFormat(s32 slot)
{
    if (slot != 0 || !mounted)
        return CARD_RESULT_NOCARD;
    if (mountResult == CARD_RESULT_IOERROR)
        return mountResult;
    const u32 generation = card.generation;
    std::memset(&card, 0, sizeof(card));
    card.generation = generation;
    dirty = true;
    return commit();
}

s32 CARDFreeBlocks(s32 slot, s32* bytes, s32* files)
{
    const s32 result = ready(slot);
    if (result != CARD_RESULT_READY)
        return result;
    if (bytes)
        *bytes = card.occupied ? 0 : SaveSize;
    if (files)
        *files = card.occupied ? 0 : 1;
    return CARD_RESULT_READY;
}

s32 CARDGetSerialNo(s32 slot, u64* serial)
{
    const s32 result = ready(slot);
    if (result == CARD_RESULT_READY && serial)
        *serial = 0x3154495657575401ULL;
    return result;
}

s32 CARDOpen(s32 slot, const char* name, CARDFileInfo* file)
{
    const s32 result = file_ready(slot, 0);
    if (result != CARD_RESULT_READY)
        return result;
    if (std::strcmp(name, "gczelda") != 0)
        return CARD_RESULT_NOFILE;
    *file = {slot, 0, 0, SaveSize, 0};
    return CARD_RESULT_READY;
}

s32 CARDCreate(s32 slot, const char* name, u32 size, CARDFileInfo* file)
{
    const s32 result = ready(slot);
    if (result != CARD_RESULT_READY)
        return result;
    if (size != SaveSize || std::strcmp(name, "gczelda") != 0)
        return CARD_RESULT_INSSPACE;
    if (card.occupied)
        return CARD_RESULT_EXIST;
    card.occupied = 1;
    card.stat = {};
    std::strcpy(card.stat.fileName, "gczelda");
    std::memcpy(card.stat.gameName, game, sizeof(game));
    std::memcpy(card.stat.company, maker, sizeof(maker));
    card.stat.length = SaveSize;
    card.stat.iconAddr = card.stat.commentAddr = ~0u;
    std::memset(card.data, 0, sizeof(card.data));
    return CARDOpen(slot, name, file);
}

s32 CARDClose(CARDFileInfo* file)
{
    const s32 result = file_ready(file->chan, file->fileNo);
    file->fileNo = -1;
    return result;
}

s32 CARDRead(const CARDFileInfo* file, void* data, s32 size, s32 offset)
{
    const s32 result = range(file, size, offset);
    if (result == CARD_RESULT_READY)
        std::memcpy(data, card.data + offset, size);
    return result;
}

s32 CARDWrite(const CARDFileInfo* file, const void* data, s32 size, s32 offset)
{
    const s32 result = range(file, size, offset);
    if (result == CARD_RESULT_READY) {
        std::memcpy(card.data + offset, data, size);
        dirty = true;
    }
    return result;
}

s32 CARDGetStatus(s32 slot, s32 number, CARDStat* status)
{
    const s32 result = file_ready(slot, number);
    if (result == CARD_RESULT_READY)
        *status = card.stat;
    return result;
}

s32 CARDSetStatus(s32 slot, s32 number, const CARDStat* status)
{
    const s32 result = file_ready(slot, number);
    if (result != CARD_RESULT_READY)
        return result;
    card.stat = *status;
    dirty = true;
    return commit();
}
}
