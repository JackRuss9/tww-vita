// A sampling profiler for the PS Vita, from sms-vita (vita/src/vita_prof.cpp),
// for the profiling build only (-DTWW_VITA_PROFILE=ON, which compiles the game
// and the GXM renderer with -finstrument-functions).
//
// The compiler calls __cyg_profile_func_enter/exit around every function of
// the port; they keep a small "shadow stack" per thread of the functions it is
// in. A thread on another core reads those stacks 1000 times a second and
// counts, per thread, the function on top (self time) and every function on
// the stack (time including what it calls). Every 20 seconds the busiest
// functions go to boot.log as addresses, which arm-vita-eabi-addr2line (or
// the ELF) turns into names.
//
// The hooks make every call more expensive, so the game runs slower in this
// build, and small functions that are called very often look heavier than
// they are; the overall picture (which subsystems, which call chains) holds.
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace pc {
void writef(int, const char* format, ...);
}

#define NOINSTR __attribute__((no_instrument_function))

namespace {

constexpr int kMaxThreads = 32;
constexpr int kMaxDepth = 256;

struct Shadow {
	std::atomic<uint32_t> key{0}; // TPIDRURO of the thread (unique per thread)
	SceUID uid = -1;
	volatile int32_t depth = 0;
	void* volatile frames[kMaxDepth];
};
Shadow g_shadows[kMaxThreads];

// The thread's user read-only ID register: set per thread by the kernel, one
// instruction to read, no system call.
NOINSTR inline uint32_t thread_key()
{
	uint32_t v;
	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(v));
	return v;
}

NOINSTR Shadow* self()
{
	const uint32_t k = thread_key();
	if (!k)
		return nullptr;
	for (int i = 0; i < kMaxThreads; i++) {
		const uint32_t have = g_shadows[i].key.load(std::memory_order_relaxed);
		if (have == k)
			return &g_shadows[i];
		if (have == 0) {
			uint32_t expected = 0;
			if (g_shadows[i].key.compare_exchange_strong(expected, k)) {
				g_shadows[i].uid = sceKernelGetThreadId();
				return &g_shadows[i];
			}
			if (expected == k)
				return &g_shadows[i];
		}
	}
	return nullptr;
}

// Counts per address, open addressing (only the sampler thread touches it).
struct Counts {
	static constexpr int kSize = 1 << 14;
	uint32_t addr[kSize];
	uint32_t n[kSize];
	NOINSTR void add(uint32_t a)
	{
		uint32_t h = (a * 2654435761u) >> 18;
		for (int probe = 0; probe < kSize; probe++, h = (h + 1) & (kSize - 1)) {
			if (addr[h] == a) {
				n[h]++;
				return;
			}
			if (addr[h] == 0) {
				addr[h] = a;
				n[h] = 1;
				return;
			}
		}
	}
	NOINSTR void clear()
	{
		memset(addr, 0, sizeof addr);
		memset(n, 0, sizeof n);
	}
};

struct ThreadCounts {
	uint32_t samples = 0;
	Counts self, total;
};
ThreadCounts* g_counts[kMaxThreads];

NOINSTR void print_top(const char* what, int thread, const Counts& c, uint32_t samples)
{
	// The 40 largest, by repeated selection (the table is small enough).
	static uint8_t taken[Counts::kSize];
	memset(taken, 0, sizeof taken);
	pc::writef(2, "[prof] thread %d %s (of %u samples):\n", thread, what, samples);
	for (int k = 0; k < 40; k++) {
		int best = -1;
		for (int i = 0; i < Counts::kSize; i++)
			if (c.addr[i] && !taken[i] && (best < 0 || c.n[i] > c.n[best]))
				best = i;
		if (best < 0 || c.n[best] * 1000u < samples) // under 0.1%
			break;
		taken[best] = 1;
		pc::writef(2, "[prof]   %5.1f%% 0x%08x\n", 100.0 * c.n[best] / samples, (unsigned)c.addr[best]);
	}
}

NOINSTR int sampler(SceSize, void*)
{
	// The program is loaded at an address of the system's choosing: this
	// function's address here, against its address in the ELF, gives the
	// offset to subtract from the addresses below.
	pc::writef(2, "[prof] sampling profiler running (1000 samples/s); sampler at %p\n", (void*)&sampler);
	uint64_t lastDump = sceKernelGetProcessTimeWide();
	void* seen[64];
	for (;;) {
		sceKernelDelayThread(1000);
		for (int t = 0; t < kMaxThreads; t++) {
			Shadow& s = g_shadows[t];
			if (!s.key.load(std::memory_order_relaxed))
				continue;
			int d = s.depth;
			if (d <= 0)
				continue;
			if (d > kMaxDepth)
				d = kMaxDepth;
			if (!g_counts[t])
				g_counts[t] = (ThreadCounts*)calloc(1, sizeof(ThreadCounts));
			ThreadCounts* c = g_counts[t];
			if (!c)
				continue;
			c->samples++;
			c->self.add((uint32_t)(uintptr_t)s.frames[d - 1]);
			// Each function once per sample, however deep it recurses.
			int nseen = 0;
			for (int i = d - 1; i >= 0 && nseen < 64; i--) {
				void* f = s.frames[i];
				bool dup = false;
				for (int j = 0; j < nseen; j++)
					if (seen[j] == f) {
						dup = true;
						break;
					}
				if (!dup) {
					seen[nseen++] = f;
					c->total.add((uint32_t)(uintptr_t)f);
				}
			}
		}
		const uint64_t now = sceKernelGetProcessTimeWide();
		if (now - lastDump >= 20 * 1000 * 1000) {
			lastDump = now;
			for (int t = 0; t < kMaxThreads; t++) {
				ThreadCounts* c = g_counts[t];
				if (!c || c->samples < 500) // threads that hardly run are left out
					continue;
				pc::writef(2, "[prof] ---- thread %d (uid 0x%x): %u samples in 20 s ----\n", t,
				        (unsigned)g_shadows[t].uid, c->samples);
				print_top("self time", t, c->self, c->samples);
				print_top("including callees", t, c->total, c->samples);
				c->samples = 0;
				c->self.clear();
				c->total.clear();
			}
		}
	}
	return 0;
}

} // namespace

extern "C" NOINSTR void __cyg_profile_func_enter(void* fn, void*)
{
	Shadow* s = self();
	if (!s)
		return;
	const int d = s->depth;
	if (d >= 0 && d < kMaxDepth)
		s->frames[d] = fn;
	s->depth = d + 1;
}

extern "C" NOINSTR void __cyg_profile_func_exit(void*, void*)
{
	Shadow* s = self();
	if (s && s->depth > 0)
		s->depth = s->depth - 1;
}

// Started from the game entry once the log is open.
extern "C" NOINSTR void tww_vita_profiler_start(void)
{
	// Core 3 (the display callback's): away from the game thread.
	const SceUID th = sceKernelCreateThread("tww_prof", sampler, 0x10000100 - 0x20, 0x8000, 0, SCE_KERNEL_CPU_MASK_USER_0 << 3, NULL);
	if (th >= 0)
		sceKernelStartThread(th, 0, NULL);
}
