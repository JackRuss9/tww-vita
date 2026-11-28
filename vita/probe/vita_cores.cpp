// CORE LAYOUT (01.95): which threads run on which of the Vita's cores.
//
// CLASSIC (up to 01.94): the game thread on core 0, the GX worker on core 1, the
// game's own threads (audio, DVD, decompression) and the DSP/AI threads on cores 0
// and 2, so they share core 0 with the game thread.
//
// MH3 (as Monster Hunter Tri's port lays its threads out): the game thread alone on
// core 0, the GX worker alone on core 1, every helper thread on two other cores:
// 2 and 3 when CapUnlocker opens core 3, else 1 and 2. Helpers always get two cores:
// pinned to one (01.60) the DSP boot hung, a thread waiting on another one of the
// same core never let it run.
//
// A start in MH3 writes ux0:data/tww/cores_pending and removes it after 120 frames; if a start finds the file, the last one did not get
// that far, and this start uses CLASSIC (logged as vita-cores fallback).

#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>

#include <cstdint>

namespace pc {
void writef(int, const char* format, ...);
}

namespace {
constexpr char kPending[] = "ux0:data/tww/cores_pending";
constexpr unsigned kCore0 = SCE_KERNEL_CPU_MASK_USER_0, kCore1 = SCE_KERNEL_CPU_MASK_USER_1,
                   kCore2 = SCE_KERNEL_CPU_MASK_USER_2, kCore3 = SCE_KERNEL_CPU_MASK_USER_0 << 3;
unsigned g_helperMask = kCore0 | kCore2;
bool g_pending;
unsigned g_frames;
} // namespace

extern "C" int tww_vita_core_layout = 1; // menu: 0 CLASSIC, 1 MH3 (next start)
extern "C" int tww_vita_core3 = 0;       // CapUnlocker opened core 3

extern "C" unsigned tww_vita_helper_cpu_mask() { return g_helperMask; }

// Called once the settings are read, before any helper thread starts.
extern "C" void tww_vita_cores_setup()
{
    const SceUID self = sceKernelGetThreadId();
    const int before = sceKernelGetThreadCpuAffinityMask(self);
    tww_vita_core3 = sceKernelChangeThreadCpuAffinityMask(self, int(kCore3)) >= 0;
    sceKernelChangeThreadCpuAffinityMask(self, before > 0 ? before : int(kCore0 | kCore1 | kCore2));

    bool fallback = false;
    if (tww_vita_core_layout == 1) {
        const SceUID old = sceIoOpen(kPending, SCE_O_RDONLY, 0);
        if (old >= 0) {
            sceIoClose(old);
            fallback = true;
        }
    }
    if (tww_vita_core_layout == 1 && !fallback) {
        g_helperMask = tww_vita_core3 ? kCore2 | kCore3 : kCore1 | kCore2;
        const SceUID file = sceIoOpen(kPending, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
        if (file >= 0)
            sceIoClose(file);
        g_pending = true;
    } else {
        g_helperMask = kCore0 | kCore2;
        if (fallback)
            sceIoRemove(kPending);  // the next start tries MH3 again
    }
    pc::writef(2, "vita-cores layout=%s core3=%d helper_mask=0x%x%s\n",
               g_helperMask == (kCore0 | kCore2) ? "classic" : "mh3", tww_vita_core3, g_helperMask,
               fallback ? " (fallback: the last MH3 start did not reach the game)" : "");
}

// Every presented frame; 120 frames (the DSP boot hang came before the first one)
// make the start count as good. 01.95-01.97 waited 600, and a start closed before
// that (a quick look at the title screen) made the next one fall back to CLASSIC.
extern "C" void tww_vita_cores_frame()
{
    if (!g_pending || ++g_frames < 120)
        return;
    g_pending = false;
    sceIoRemove(kPending);
    pc::writef(2, "vita-cores start-confirmed\n");
}
