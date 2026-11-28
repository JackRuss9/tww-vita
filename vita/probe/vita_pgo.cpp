// PGO training build (TWW_VITA_PGO=generate, 01.97).
//
// The instrumented code is built with -fprofile-info-section=gcov_info: GCC puts a
// pointer to every object's gcov_info in the gcov_info section and registers no
// exit-time dump (the game never exits cleanly, and libgcov's file I/O would not
// find the build paths on the Vita anyway). tww_vita_pgo_dump() serializes all of
// them with __gcov_info_to_gcda into one stream file, ux0:data/tww/pgo.stream;
// `gcov-tool merge-stream` turns it back into .gcda files on the build machine.
//
// The counters only grow, so each dump replaces the file with the whole profile so
// far: a dump every 2 minutes of frames and whenever the settings menu closes.

#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace pc {
void writef(int, const char* format, ...);
}

#ifdef TWW_VITA_PGO_GENERATE
struct gcov_info;
extern "C" {
void __gcov_info_to_gcda(const gcov_info* info, void (*filename)(const char*, void*),
                         void (*dump)(const void*, unsigned, void*), void* (*allocate)(unsigned, void*),
                         void* arg);
void __gcov_filename_to_gcfn(const char* filename, void (*dump)(const void*, unsigned, void*), void* arg);
extern const gcov_info* const __start_gcov_info[];
extern const gcov_info* const __stop_gcov_info[];
}

namespace {
constexpr char kStream[] = "ux0:data/tww/pgo.stream";
constexpr char kTemp[] = "ux0:data/tww/pgo.stream.tmp";

struct Writer {
    SceUID file;
    uint8_t* buffer;
    unsigned used;
    unsigned long long total;
    bool failed;
    void* blocks[64];  // __gcov_info_to_gcda's allocations for one object
    unsigned blockCount;
};
constexpr unsigned kBuffer = 256 * 1024;

void flush(Writer* w)
{
    if (w->used && !w->failed && sceIoWrite(w->file, w->buffer, w->used) != int(w->used))
        w->failed = true;
    w->total += w->used;
    w->used = 0;
}

void dump(const void* data, unsigned length, void* arg)
{
    Writer* w = static_cast<Writer*>(arg);
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    while (length) {
        if (w->used == kBuffer)
            flush(w);
        unsigned n = kBuffer - w->used;
        if (n > length)
            n = length;
        std::memcpy(w->buffer + w->used, bytes, n);
        w->used += n;
        bytes += n;
        length -= n;
    }
}

void filename(const char* name, void* arg) { __gcov_filename_to_gcfn(name, dump, arg); }

void* allocate(unsigned length, void* arg)
{
    Writer* w = static_cast<Writer*>(arg);
    void* p = std::malloc(length);
    if (p && w->blockCount < 64)
        w->blocks[w->blockCount++] = p;
    return p;
}

unsigned g_frames;
} // namespace

extern "C" void tww_vita_pgo_dump(const char* why)
{
    const uint64_t start = sceKernelGetProcessTimeWide();
    Writer w{};
    w.buffer = static_cast<uint8_t*>(std::malloc(kBuffer));
    w.file = sceIoOpen(kTemp, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (!w.buffer || w.file < 0) {
        pc::writef(2, "vita-pgo dump failed: cannot open %s\n", kTemp);
        std::free(w.buffer);
        return;
    }
    unsigned objects = 0;
    for (const gcov_info* const* info = __start_gcov_info; info != __stop_gcov_info; ++info) {
        if (!*info)
            continue;
        w.blockCount = 0;
        __gcov_info_to_gcda(*info, filename, dump, allocate, &w);
        for (unsigned i = 0; i < w.blockCount; ++i)
            std::free(w.blocks[i]);
        ++objects;
    }
    flush(&w);
    sceIoClose(w.file);
    std::free(w.buffer);
    if (w.failed) {
        pc::writef(2, "vita-pgo dump failed while writing (%s)\n", why);
        return;
    }
    sceIoRemove(kStream);
    sceIoRename(kTemp, kStream);
    pc::writef(2, "vita-pgo dump %s: %u objects, %llu KB in %llu ms\n", why, objects, w.total / 1024,
               (sceKernelGetProcessTimeWide() - start) / 1000);
}

extern "C" void tww_vita_pgo_frame()
{
    if (++g_frames % 3600 == 0)  // ~2 min at 30 fps, longer when slower
        tww_vita_pgo_dump("timer");
}
extern "C" int tww_vita_pgo_build() { return 1; }
#else
extern "C" void tww_vita_pgo_dump(const char*) {}
extern "C" void tww_vita_pgo_frame() {}
extern "C" int tww_vita_pgo_build() { return 0; }
#endif
