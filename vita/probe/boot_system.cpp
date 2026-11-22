#include "disc.h"
#include "gpu.h"
#include <aurora/aurora.h>
#include <dolphin/arq.h>
#include <dolphin/os.h>
#include "cos_sdk/hooks.h"
#include "JSystem/JFramework/JFWSystem.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRDvdFile.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JSystem/JUtility/JUTConsole.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

bool mDoMch_CreateGameHeaps(u32 commandBytes, u32 archiveBytes, u32 gameBytes, JKRHeap *system);
JKRExpHeap *mDoExt_getGameHeap();
JKRExpHeap *mDoExt_getZeldaHeap();
extern "C" int vita_join_alarm_thread();

namespace {
constexpr unsigned int ArenaBytes = 128 * 1024 * 1024;
constexpr unsigned int AramBytes = 16 * 1024 * 1024;
SceUID boot_log = -1;
JKRExpHeap *game_heap;

struct SchedulerCheck {
    OSThread thread;
    alignas(32) unsigned char stack[32 * 1024];
    OSMessageQueue queue;
    OSMessage messages[2];
    OSAlarm alarm;
};
SchedulerCheck scheduler{};
int worker_message, alarm_message;
u32 aram_allocations[4];
uintptr_t aram_completed;

void aram_done(uintptr_t request)
{
    aram_completed = request;
}

void *scheduler_worker(void *)
{
    void *block = game_heap->alloc(4096, 32);
    if (!block)
        return nullptr;
    std::memset(block, 0xa5, 4096);
    game_heap->free(block);
    OSSendMessage(&scheduler.queue, &worker_message, OS_MESSAGE_NOBLOCK);
    return &worker_message;
}

void system_checkpoint(SceUID log, const char *message)
{
    probe_log(log, "%s\n", message);
    if (log >= 0)
        sceIoSyncByFd(log, 0);
}

void scheduler_alarm(OSAlarm *, OSContext *)
{
    OSSendMessage(&scheduler.queue, &alarm_message, OS_MESSAGE_NOBLOCK);
}

void report(const char *format, va_list args)
{
    char message[160];
    std::vsnprintf(message, sizeof(message), format, args);
    probe_log(boot_log, "%s", message);
}
}

extern "C" void probe_scheduler_diagnostic(const char *message, bool fatal)
{
    probe_log(boot_log, "scheduler%s: %s\n", fatal ? " fatal" : "", message);
    if (fatal && boot_log >= 0)
        sceIoSyncByFd(boot_log, 0);
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
    probe_log(boot_log, "check: SwitchWaker scheduler SKIP (OS panic)\n");
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
    aurora::g_config.mem2Size = AramBytes;
    probe_log(log, "boot: entering Aurora OSInit\n");
    OSInit();
    const auto lo = reinterpret_cast<uintptr_t>(OSGetArenaLo());
    const auto hi = reinterpret_cast<uintptr_t>(OSGetArenaHi());
    if (lo == 0 || hi <= lo || hi - lo != ArenaBytes - 0x4000 ||
        static_cast<const OSBootInfo *>(OSPhysicalToCached(0))->memorySize != ArenaBytes)
        return -1;
    probe_log(log, "boot: OS arena ready (%u bytes)\n", static_cast<unsigned int>(hi - lo));
    probe_log(log, "boot: OS clock %lld ticks\n", static_cast<long long>(OSGetTime()));

    constexpr unsigned int SystemBytes = 32 * 1024 * 1024;
    constexpr unsigned int CommandBytes = 0x1000 * 2;
    constexpr unsigned int ArchiveBytes = 0xa51400 * 2;
    constexpr unsigned int GameBytes = 0x2ce800 * 20;
    JFWSystem::setMaxStdHeap(1);
    JFWSystem::setSysHeapSize(SystemBytes);
    system_checkpoint(log, "boot: entering JFWSystem::firstInit");
    JFWSystem::firstInit();
    JKRExpHeap *root = JFWSystem::rootHeap;
    JKRExpHeap *system = JFWSystem::getSystemHeap();
    if (!root || !system || !root->check() || !system->check())
        return -1;
    system_checkpoint(log, "boot: entering mDoMch_CreateGameHeaps");
    if (!mDoMch_CreateGameHeaps(CommandBytes, ArchiveBytes, GameBytes, system) ||
        !root->check() || !system->check() || !mDoExt_getZeldaHeap()->check())
        return -1;
    JKRExpHeap *game = mDoExt_getGameHeap();
    probe_log(log, "boot: mDoMch game heaps ready, root free %u, system free %u, Zelda free %u\n",
        root->getTotalFreeSize(), system->getTotalFreeSize(), mDoExt_getZeldaHeap()->getTotalFreeSize());

    const u32 free_before = game->getTotalFreeSize();
    void *sample = game->alloc(4096, 32);
    if (!sample || (reinterpret_cast<uintptr_t>(sample) & 31) != 0)
        return -1;
    std::memset(sample, 0x5a, 4096);
    game->free(sample);
    if (game->getTotalFreeSize() != free_before || !game->check())
        return -1;
    probe_log(log, "boot: JKR allocation and free pass\n");
    game_heap = game;
    return 0;
}

extern "C" int boot_probe_aram(SceUID log)
{
    if (ARInit(aram_allocations, sizeof(aram_allocations) / sizeof(aram_allocations[0])) == 0 ||
        ARGetSize() != AramBytes)
        return -1;
    ARQInit();
    alignas(32) unsigned char source[256], result[256]{};
    for (unsigned int i = 0; i < sizeof(source); ++i)
        source[i] = static_cast<unsigned char>(i ^ 0xa5);
    const u32 address = ARAlloc(sizeof(source));
    if (address == 0)
        return -1;
    ARQRequest upload{}, download{};
    system_checkpoint(log, "aram: main RAM to ARAM");
    ARQPostRequest(&upload, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH,
                   reinterpret_cast<uintptr_t>(source), address, sizeof(source), aram_done);
    const bool sent = aram_completed == reinterpret_cast<uintptr_t>(&upload);
    system_checkpoint(log, "aram: ARAM to main RAM");
    ARQPostRequest(&download, 0, ARQ_TYPE_ARAM_TO_MRAM, ARQ_PRIORITY_HIGH,
                   address, reinterpret_cast<uintptr_t>(result), sizeof(result), aram_done);
    const bool received = aram_completed == reinterpret_cast<uintptr_t>(&download);
    u32 freed = 0;
    const u32 mark = ARFree(&freed);
    probe_log(log, "aram: capacity %u, offset 0x%08x, returned %u bytes\n", ARGetSize(), address, freed);
    return sent && received && mark == address && freed == sizeof(source) &&
        std::memcmp(source, result, sizeof(source)) == 0 ? 0 : -1;
}

extern "C" int boot_probe_stop_scheduler(SceUID log)
{
    if (!game_heap)
        return 0;
    system_checkpoint(log, "shutdown: requesting SDK shutdown");
    COSSdkRequestShutdown();
    system_checkpoint(log, "shutdown: joining alarm timer");
    const int error = vita_join_alarm_thread();
    probe_log(log, "shutdown: alarm timer join result %d\n", error);
    return error;
}

extern "C" int boot_probe_game_dvd(SceUID log)
{
    constexpr const char *resources[] = {
        "/res/Stage/sea_T/Stage.arc",
        "/res/Stage/sea/LODALL.arc",
    };
    bool ok = DVDConvertPathToEntrynum("/__tww_missing__") < 0;
    for (const char *resource : resources) {
        system_checkpoint(log, resource);
        const s32 entry = DVDConvertPathToEntrynum(resource);
        JKRDvdFile by_index(entry);
        JKRDvdFile by_path(resource);
        alignas(32) unsigned char indexed[32], named[32];
        const s32 first = by_index.isAvailable() ? by_index.readData(indexed, sizeof(indexed), 0) : -1;
        const s32 second = by_path.isAvailable() ? by_path.readData(named, sizeof(named), 0) : -1;
        const bool match = first == 32 && second == 32 &&
            (std::memcmp(indexed, "Yaz0", 4) == 0 || std::memcmp(indexed, "RARC", 4) == 0) &&
            std::memcmp(indexed, named, sizeof(indexed)) == 0 &&
            by_index.getFileSize() == by_path.getFileSize();
        probe_log(log, "game DVD: entry %d, size %d, path read %d, indexed read %d, match %u\n",
            entry, by_index.isAvailable() ? by_index.getFileSize() : -1, second, first,
            static_cast<unsigned int>(match));
        ok &= match;
    }
    return ok ? 0 : -1;
}

extern "C" int boot_probe_scheduler(SceUID log)
{
    system_checkpoint(log, "scheduler: adopting main thread");
    if (!OSGetCurrentThread()) {
        probe_log(log, "scheduler: could not adopt main thread\n");
        return -1;
    }
    OSInitMessageQueue(&scheduler.queue, scheduler.messages, 2);
    OSCreateAlarm(&scheduler.alarm);
    const u32 free_before = game_heap->getTotalFreeSize();
    system_checkpoint(log, "scheduler: creating worker");
    if (!OSCreateThread(&scheduler.thread, scheduler_worker, nullptr,
                        scheduler.stack + sizeof(scheduler.stack), sizeof(scheduler.stack), 16, 0)) {
        probe_log(log, "scheduler: OSCreateThread failed\n");
        return -1;
    }

    system_checkpoint(log, "scheduler: resuming worker and arming 20 ms alarm");
    OSResumeThread(&scheduler.thread);
    OSSetAlarm(&scheduler.alarm, OSMillisecondsToTicks(20), scheduler_alarm);
    bool worker = false, alarm = false;
    for (unsigned int i = 0; i < 1000; ++i) {
        OSMessage message;
        while (OSReceiveMessage(&scheduler.queue, &message, OS_MESSAGE_NOBLOCK)) {
            worker |= message == &worker_message;
            alarm |= message == &alarm_message;
        }
        if (worker && alarm && OSIsThreadTerminated(&scheduler.thread))
            break;
        sceKernelDelayThread(2000);
    }
    OSCancelAlarm(&scheduler.alarm);
    const bool terminated = OSIsThreadTerminated(&scheduler.thread);
    void *result = nullptr;
    const bool joined = terminated && OSJoinThread(&scheduler.thread, &result);
    const bool heap_ok = joined && game_heap->getTotalFreeSize() == free_before && game_heap->check();
    probe_log(log, "scheduler: worker message %u, alarm message %u, joined %u, shared heap %u\n",
        static_cast<unsigned int>(worker), static_cast<unsigned int>(alarm),
        static_cast<unsigned int>(joined && result == &worker_message), static_cast<unsigned int>(heap_ok));
    return worker && alarm && result == &worker_message && heap_ok ? 0 : -1;
}
