#include "pc/game_hooks.h"
#include "pc/pc_gpu_opts.h"
#include "pc/pc_harness.h"
#include "pc/pc_controls.h"
#include "pc_internal.h"

#include <dolphin/mtx.h>
#include <dolphin/os.h>
#include <dolphin/si.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <sys/time.h>


extern "C" OSTime OSGetSystemTime()
{
    constexpr OSTime GameCubeEpoch = 946684800;
    timeval utc{};
    if (gettimeofday(&utc, nullptr) != 0)
        return OSGetTime();
    return (static_cast<OSTime>(utc.tv_sec) - GameCubeEpoch) * OS_TIMER_CLOCK
        + static_cast<OSTime>(utc.tv_usec) * OS_TIMER_CLOCK / 1000000;
}

extern "C" u32 SIProbe(s32 channel)
{
    return channel == 0 ? SI_GC_CONTROLLER : SI_ERROR_NO_RESPONSE;
}

extern "C" void pc_frame_pace(unsigned long long period_ns)
{
    static uint64_t previous_us;
    const uint64_t deadline_us = previous_us + period_ns / 1000;
    const uint64_t now_us = sceKernelGetProcessTimeWide();
    if (now_us < deadline_us)
        sceKernelDelayThread(static_cast<unsigned int>(deadline_us - now_us));
    previous_us = sceKernelGetProcessTimeWide();
}

extern "C" void pc_heaps_created() { pc_milestone("vita-game-heaps-ready"); }
extern "C" void pc_copydate_loaded(int status, const char *date)
{
    pc::writef(2, "COPYDATE status=%d date=%.64s\n", status, date ? date : "");
    pc_milestone("vita-copydate-read-complete");
}

void pc_logo_scene_created_hook() { pc_milestone("vita-logo-scene-created"); }
bool pc_logo_dvd_synced(dScnLogo_c *)
{
    static bool logged;
    if (!logged) {
        logged = true;
        pc_milestone("vita-logo-resources-synced");
    }
    return false;
}

extern "C" void pc_opening_scene_called()
{
    static bool logged;
    if (!logged) {
        logged = true;
        pc_milestone("vita-opening-scene-requested");
    }
}
extern "C" void pc_stage_created(const char *stage, int room, int files, int dzs)
{
    pc::writef(2, "stage %.8s room=%d files=%d dzs=%d\n", stage, room, files, dzs);
    pc_milestone("vita-stage-created");
}

extern "C" void aurora_vita_request_title_probe();

extern "C" void pc_title_drawn()
{
    static bool logged;
    if (!logged) {
        logged = true;
        pc_milestone("vita-title-draw-submitted");
        aurora_vita_request_title_probe();
    }
}

extern "C" void pc_name_scene_drawn(int main_proc, int card_proc, int draw_proc)
{
    static int last_main = -1, last_card = -1, last_draw = -1;
    if (main_proc != last_main || card_proc != last_card || draw_proc != last_draw) {
        last_main = main_proc;
        last_card = card_proc;
        last_draw = draw_proc;
        pc::writef(2, "file-select main=%d card=%d draw=%d\n", main_proc, card_proc, draw_proc);
        pc_milestone("vita-file-select-draw");
    }
}

extern "C" void pc_play_stage_started(const char *stage, int room, int point, int layer)
{
    pc::writef(2, "play stage %.8s room=%d point=%d layer=%d\n", stage, room, point, layer);
    pc_milestone("vita-play-stage-started");
}

extern "C" void pc_trace_scene(int process)
{
    pc::writef(2, "scene process=0x%x\n", process);
    pc_milestone("vita-scene-request");
}

extern "C" void pc_trace_resource(const char *path, int entry)
{
    pc::writef(2, "resource entry=%d path=%s\n", entry, path ? path : "");
}

extern "C" void tww_vita_heap_overlap(const void* heap, const void* block, u32 size,
                                       const void* next, u32 next_size, uintptr_t end, uintptr_t next_start)
{
    pc::writef(2, "heap overlap heap=%p block=%p size=0x%x next=%p next_size=0x%x end=0x%lx next_start=0x%lx\n",
               heap, block, unsigned(size), next, unsigned(next_size),
               static_cast<unsigned long>(end), static_cast<unsigned long>(next_start));
    pc_milestone("vita-heap-overlap");
}

extern "C" [[noreturn]] void pc_panic(const char *file, int line)
{
    pc::writef(2, "game panic %s:%d\n", file, line);
    pc::writef(2, "panic anchor=0x%08lx\n", static_cast<unsigned long>(reinterpret_cast<uintptr_t>(&pc_panic)));

    // The Vita toolchain does not expose a uniform unwind API for game objects.
    // Record candidate code addresses within the current thread's stack bounds.
    SceKernelThreadInfo info{};
    info.size = sizeof(info);
    if (sceKernelGetThreadInfo(sceKernelGetThreadId(), &info) >= 0 && info.stackSize > 0) {
        uintptr_t sp;
        asm volatile("mov %0, sp" : "=r"(sp));
        const uintptr_t base = reinterpret_cast<uintptr_t>(info.stack);
        const uintptr_t top = base + static_cast<uintptr_t>(info.stackSize);
        if (sp >= base && sp < top) {
            for (unsigned i = 0; i < 256 && top - sp >= (i + 1) * sizeof(uintptr_t); ++i) {
                const uintptr_t word = reinterpret_cast<const volatile uintptr_t*>(sp)[i];
                if (word >= 0x81000000U && word < 0x82000000U)
                    pc::writef(2, "panic stack[%u]=0x%08lx\n", i, static_cast<unsigned long>(word));
            }
        }
    }
    pc_milestone("vita-panic-stack-complete");
    pc_exit(12);
}

// Hardware input is read by vita_pad.cpp. These hooks belong to desktop input injection.
extern "C" void pc_pad_feed() {}
extern "C" void pc_pad_read_done() {}
extern "C" int pc_camera_override(float *, float *) { return 0; }
extern "C" void pc_aim_gyro_reset() {}
extern "C" int pc_aim_gyro_turn(int, float *, float *) { return 0; }

// Render at the stock resolution and pace, with one paint per game step.
extern "C" int pc_frame_split() { return 0; }
extern "C" unsigned int pc_frame_wait_retraces(unsigned int count) { return count; }
extern "C" void pc_frame_draw_end() {}
extern "C" void pc_paint_extra_begin() {}
extern "C" void pc_paint_extra_end() {}
extern "C" int pc_paint_is_extra() { return 0; }
extern "C" void pc_dynres_3d_begin() {}
extern "C" void pc_dynres_3d_end() {}
extern "C" int pc_dof_enabled() { return 0; }
extern "C" {
int pc_gpu_groups_level = 0;
void pc_gpu_group(const char *) {}
void pc_gpu_group_packet(const void *) {}
void pc_perf_begin(int phase)
{
    static bool game_logged;
    static bool painter_logged;
    if (phase == PC_PERF_GAME && !game_logged) {
        game_logged = true;
        pc_milestone("vita-first-game-update-begin");
    } else if (phase == PC_PERF_PAINTER && !painter_logged) {
        painter_logged = true;
        pc_milestone("vita-first-painter-begin");
    }
}

void pc_perf_end(int phase)
{
    static bool game_logged;
    static bool painter_logged;
    if (phase == PC_PERF_GAME && !game_logged) {
        game_logged = true;
        pc_milestone("vita-first-game-update-complete");
    } else if (phase == PC_PERF_PAINTER && !painter_logged) {
        painter_logged = true;
        pc_milestone("vita-first-painter-complete");
    }
}
void pc_play_draw()
{
    static bool logged;
    if (!logged) {
        logged = true;
        pc_milestone("vita-first-play-draw");
    }
}
}

void pc_kyr_draw_mist(PcFnRef draw) { draw(); }

// GXM keeps EFB copies on the GPU. The photo identification pass sees an empty shutter region.
void pc_efb_peek_request(u16, u16, u16, u16, void (*done)()) { if (done) done(); }
u32 pc_efb_peek_argb(u16, u16) { return 0xffffffff; }
bool pc_gph_capture_ready(void *, u32, u32, int) { return false; }
void pc_snap_judged(int, const dSnap_RegistObjElm *, int) {}
void pc_mail_sort_merged(J3DMatPacket *, J3DMatPacket *) {}
bool pc_mail_sort_start() { return false; }
bool pc_mail_sort_throw() { return false; }

void pc_fps60_camera_drawn(view_class *) {}
void pc_fps60_camera_reset() {}
const float *pc_fps60_line_positions(const void *, int, const float *current, const float *, unsigned int)
{
    return current;
}
void pc_fps60_model_init(J3DModel *) {}
void pc_fps60_model_viewcalc(J3DModel *) {}
void pc_fps60_models_draw_begin() {}
void pc_fps60_models_draw_end() {}
int pc_fps60_models_skin_delta(J3DModel *, float (*)[4]) { return 0; }
void pc_fps60_packet_mtx(const void *, const float current[3][4], float out[3][4])
{
    MTXCopy(current, out);
}
const float *pc_fps60_paint_floats(const void *, const float *current, unsigned int) { return current; }
void pc_fps60_paint_prepare(view_class *) {}
void pc_fps60_painting(int) {}
int pc_fps60_particles_emitter_begin(JPABaseEmitter *) { return 0; }
void pc_fps60_particles_emitter_end(JPABaseEmitter *) {}
void pc_fps60_particles_emitter_turn(JPABaseEmitter *, float *) {}
void pc_fps60_particles_register(JPAEmitterManager *, JPABaseParticle *, unsigned int, JPABaseEmitter *, unsigned int) {}
float pc_fps60_particles_strip_shift(JPABaseEmitter *) { return 0; }
float pc_fps60_sea_t() { return 0; }
void pc_fps60_sky_begin() {}
void pc_fps60_sky_end() {}
void pc_fps60_view_begin(view_class *) {}
void pc_fps60_view_end() {}
unsigned char pc_fps60_blur_rate(unsigned char rate, const float current[3][4], float out[3][4])
{
    MTXCopy(current, out);
    return rate;
}
