#include "disc.h"
#include "gpu.h"
#include <aurora/aurora.h>
#include <dolphin/os.h>
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JSystem/JUtility/JUTConsole.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
constexpr unsigned int ArenaBytes = 128 * 1024 * 1024;
SceUID boot_log = -1;

void report(const char *format, va_list args)
{
    char message[160];
    std::vsnprintf(message, sizeof(message), format, args);
    probe_log(boot_log, "%s", message);
}
}

namespace aurora {
AuroraConfig g_config{};
}

uintptr_t OSBaseAddress;
void *MEM1Start;
void *MEM1End;

void AuroraOSInitMemory()
{
    const auto size = aurora::g_config.mem1Size;
    SceUID block = sceKernelAllocMemBlock("SwitchWaker MEM1", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, nullptr);
    if (block < 0) {
        probe_log(boot_log, "boot: MEM1 allocation failed 0x%08x\n", static_cast<unsigned int>(block));
        return;
    }
    void *base = nullptr;
    const int result = sceKernelGetMemBlockBase(block, &base);
    if (result < 0) {
        sceKernelFreeMemBlock(block);
        probe_log(boot_log, "boot: MEM1 base failed 0x%08x\n", static_cast<unsigned int>(result));
        return;
    }
    MEM1Start = base;
    MEM1End = static_cast<unsigned char *>(base) + size;
    OSBaseAddress = reinterpret_cast<uintptr_t>(base);
    // JKR retains arena pointers until the kernel reclaims the block on exit.
}

void AuroraInitClock()
{
    if (OSBaseAddress != 0) {
        __OSBusClock = OS_BUS_CLOCK;
        __OSCoreClock = OS_CORE_CLOCK;
    }
}

OSTime OSGetTime()
{
    const uint64_t usec = sceKernelGetProcessTimeWide();
    return static_cast<OSTime>(usec * (OS_TIMER_CLOCK / 100000) / 10);
}

// This probe runs JKR on one thread, before the game scheduler starts.
void OSInitMutex(OSMutex *mutex) { *mutex = {}; }
void OSLockMutex(OSMutex *mutex) { ++mutex->count; }
void OSUnlockMutex(OSMutex *mutex) { --mutex->count; }

void OSReport(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(format, args);
    va_end(args);
}

void OSPanic(const char *file, int line, const char *format, ...)
{
    probe_log(boot_log, "boot: OS panic at %s:%d\n", file, line);
    va_list args;
    va_start(args, format);
    report(format, args);
    va_end(args);
    probe_log(boot_log, "check: Aurora OS and JKR heaps FAIL (OS panic)\n");
    probe_log(boot_log, "check: GXM texture and framebuffer SKIP (OS panic)\n");
    boot_probe_exit(boot_log, -1);
}

void JUTReportConsole(const char *message) { OSReport("%s", message); }
void JUTWarningConsole(const char *message) { OSReport("%s", message); }

void JUTReportConsole_f(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(format, args);
    va_end(args);
}

void JUTWarningConsole_f(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(format, args);
    va_end(args);
}

namespace JUTAssertion {
u32 getSDevice() { return 0; }

void showAssert(u32, const char *file, int line, const char *condition)
{
    probe_log(boot_log, "boot: JKR assert at %s:%d (%s)\n", file, line, condition);
}

void setWarningMessage_f(u32, char *file, int line, const char *format, ...)
{
    probe_log(boot_log, "boot: JKR warning at %s:%d\n", file, line);
    va_list args;
    va_start(args, format);
    report(format, args);
    va_end(args);
}

void setLogMessage_f(u32, char *, int, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(format, args);
    va_end(args);
}
}

void JKRPcReportAllocFailure(JKRHeap *, u32 size, int alignment)
{
    probe_log(boot_log, "boot: JKR allocation failed (%u bytes, alignment %d)\n", size, alignment);
}

extern "C" int boot_probe_system(SceUID log)
{
    boot_log = log;
    aurora::g_config.mem1Size = ArenaBytes;
    probe_log(log, "boot: entering Aurora OSInit\n");
    OSInit();
    const auto lo = reinterpret_cast<uintptr_t>(OSGetArenaLo());
    const auto hi = reinterpret_cast<uintptr_t>(OSGetArenaHi());
    if (lo == 0 || hi <= lo || hi - lo != ArenaBytes - 0x4000 ||
        static_cast<const OSBootInfo *>(OSPhysicalToCached(0))->memorySize != ArenaBytes)
        return -1;
    probe_log(log, "boot: OS arena ready (%u bytes)\n", static_cast<unsigned int>(hi - lo));
    probe_log(log, "boot: OS clock %lld ticks\n", static_cast<long long>(OSGetTime()));

    probe_log(log, "boot: entering JKRExpHeap::createRoot\n");
    JKRExpHeap *root = JKRExpHeap::createRoot(1, false);
    if (!root || !root->check())
        return -1;
    probe_log(log, "boot: JKR root heap ready\n");

    constexpr unsigned int SystemBytes = 32 * 1024 * 1024;
    constexpr unsigned int ArchiveBytes = 0xa51400 * 2;
    constexpr unsigned int GameBytes = 0x2ce800 * 20;
    JKRExpHeap *system = JKRExpHeap::create(SystemBytes, root, false);
    JKRExpHeap *archive = JKRExpHeap::create(ArchiveBytes, root, false);
    JKRExpHeap *game = JKRExpHeap::create(GameBytes, root, false);
    if (!system || !archive || !game || !root->check())
        return -1;
    probe_log(log, "boot: JKR system/archive/game heaps ready\n");

    const u32 free_before = game->getTotalFreeSize();
    void *sample = game->alloc(4096, 32);
    if (!sample || (reinterpret_cast<uintptr_t>(sample) & 31) != 0)
        return -1;
    std::memset(sample, 0x5a, 4096);
    game->free(sample);
    if (game->getTotalFreeSize() != free_before || !game->check())
        return -1;
    probe_log(log, "boot: JKR allocation and free pass\n");
    return 0;
}
