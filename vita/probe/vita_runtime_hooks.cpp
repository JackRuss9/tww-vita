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
#include <cstdio>
#include <cstring>


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
// CPU PROFILE (vita_menu.cpp): the painter's time between its PC_GPU_GROUP markers, and each
// game process's own execute and draw time (f_pc_manager.cpp), reported every 10 s.
int tww_vita_cpu_profile = 0;
unsigned long long tww_vita_prof_now(void) { return sceKernelGetProcessTimeWide(); }
namespace {
constexpr int kProfNames = 1024;
unsigned long long g_profProc[2][kProfNames];
unsigned int g_profCount[2][kProfNames];
unsigned long long g_profSection[3];  // execute pass, draw pass, after-draw callback
struct Bucket { const char *name; unsigned long long us; };
Bucket g_buckets[40];
int g_bucketCount;
const char *g_bucketName;
unsigned long long g_bucketStart;
void bucket_close(unsigned long long now)
{
    if (!g_bucketName)
        return;
    int i = 0;
    while (i < g_bucketCount && g_buckets[i].name != g_bucketName)
        ++i;
    if (i == g_bucketCount) {
        if (g_bucketCount == 40)
            return;
        g_buckets[g_bucketCount++] = {g_bucketName, 0};
    }
    g_buckets[i].us += now - g_bucketStart;
}
}
// Play scene marks (d_s_play.cpp): the time since the previous mark goes to the mark's name.
Bucket g_scn[40];
int g_scnCount;
unsigned long long g_scnLast;
void tww_vita_scn_begin(void)
{
    if (tww_vita_cpu_profile)
        g_scnLast = sceKernelGetProcessTimeWide();
}
void tww_vita_scn_mark(const char *name)
{
    if (!tww_vita_cpu_profile)
        return;
    const unsigned long long now = sceKernelGetProcessTimeWide();
    int i = 0;
    while (i < g_scnCount && g_scn[i].name != name && std::strcmp(g_scn[i].name, name) != 0)
        ++i;
    if (i == g_scnCount) {
        if (g_scnCount == 40)
            return;
        g_scn[g_scnCount++] = {name, 0};
    }
    if (g_scnLast)
        g_scn[i].us += now - g_scnLast;
    g_scnLast = now;
}
// J3DMatPacket::draw (J3DPacket.cpp): 0 material load, 1 shape matrix loads, 2 shape lists,
// 3 material packets, 4 shape packets.
unsigned long long g_slots[8];
void tww_vita_prof_slot(int slot, unsigned long long value)
{
    if (slot >= 0 && slot < 8)
        g_slots[slot] += value;
}
// Top-level draw-buffer packets by vtable (J3DDrawBuffer.cpp).
struct VtEntry { void *vt; unsigned long long us; unsigned count; };
VtEntry g_vt[64];
void tww_vita_prof_packet(void *vt, unsigned long long us)
{
    unsigned h = (reinterpret_cast<uintptr_t>(vt) >> 2) & 63;
    for (unsigned k = 0; k < 64; ++k, h = (h + 1) & 63) {
        if (g_vt[h].vt == vt || g_vt[h].vt == nullptr) {
            g_vt[h].vt = vt;
            g_vt[h].us += us;
            ++g_vt[h].count;
            return;
        }
    }
}
void aurora_vita_worker_prof(unsigned long long *out, unsigned count);
void aurora_vita_set_worker_prof(int enabled);
void tww_vita_prof_proc(int name, int draw, unsigned long long us)
{
    if (name < 0) {
        g_profSection[name == -2 ? 2 : draw] += us;
        return;
    }
    if (name >= kProfNames)
        name = kProfNames - 1;
    g_profProc[draw][name] += us;
    ++g_profCount[draw][name];
}
void pc_gpu_group(const char *name)
{
    if (!tww_vita_cpu_profile || !g_bucketName)
        return;
    const unsigned long long now = sceKernelGetProcessTimeWide();
    bucket_close(now);
    g_bucketName = name;
    g_bucketStart = now;
}
// Called by the 10 s report in vita_gxm_frames.cpp: per-frame 0.1 ms figures, then reset.
void tww_vita_prof_report(int fd, unsigned int frames,
                          void (*log)(int, const char *, ...))
{
    if (!tww_vita_cpu_profile || frames == 0)
        return;
    const auto per = [&](unsigned long long us) { return static_cast<unsigned long>(us / frames / 100); };
    log(fd, "vita-cpu-0.1ms execute=%lu draw_pass=%lu after_draw_cb=%lu\n", per(g_profSection[0]),
        per(g_profSection[1]), per(g_profSection[2]));
    char line[512];
    int n = std::snprintf(line, sizeof line, "vita-cpu-painter-0.1ms");
    for (int i = 0; i < g_bucketCount && n < 480; ++i)
        n += std::snprintf(line + n, sizeof line - n, " %s=%lu", g_buckets[i].name, per(g_buckets[i].us));
    log(fd, "%s\n", line);
    for (int draw = 0; draw < 2; ++draw) {
        // The 14 costliest process names (hex, d_procname.h), time per frame and calls per frame.
        bool used[kProfNames]{};
        n = std::snprintf(line, sizeof line, "vita-cpu-%s-0.01ms", draw ? "draw" : "exec");
        for (int k = 0; k < 14; ++k) {
            int best = -1;
            for (int i = 0; i < kProfNames; ++i)
                if (!used[i] && g_profProc[draw][i] && (best < 0 || g_profProc[draw][i] > g_profProc[draw][best]))
                    best = i;
            if (best < 0)
                break;
            used[best] = true;
            n += std::snprintf(line + n, sizeof line - n, " %x=%lu/%u", best,
                               static_cast<unsigned long>(g_profProc[draw][best] / frames / 10),
                               (g_profCount[draw][best] + frames / 2) / frames);
        }
        log(fd, "%s\n", line);
    }
    // The GX worker (Aurora command_processor.cpp lists the slots): one command in eight is
    // timed, so those times are scaled by 8; counts are per frame.
    unsigned long long w[24];
    aurora_vita_worker_prof(w, 24);
    const auto s8 = [&](unsigned i) { return per(w[i] * 8); };
    const auto cnt = [&](unsigned i) { return static_cast<unsigned long>(w[i] / frames); };
    log(fd, "vita-worker-0.1ms merged=%lu unmerged=%lu bp=%lu cp=%lu xf=%lu xf_indexed=%lu other=%lu render=%lu present=%lu\n",
        s8(0), s8(1), s8(2), s8(3), s8(4), s8(5), s8(6), per(w[19]), per(w[20]));
    log(fd, "vita-worker-count merged=%lu unmerged=%lu bp=%lu cp=%lu xf=%lu xf_indexed=%lu\n",
        cnt(7), cnt(8), cnt(9), cnt(10), cnt(11), cnt(12));
    log(fd, "vita-worker-unmerged-0.1ms arrays=%lu pipeline=%lu textures=%lu bind_groups=%lu uniforms=%lu record=%lu\n",
        s8(13), s8(14), s8(15), s8(16), s8(17), s8(18));
    n = std::snprintf(line, sizeof line, "vita-cpu-scene-0.1ms");
    for (int i = 0; i < g_scnCount && n < 480; ++i)
        n += std::snprintf(line + n, sizeof line - n, " %s=%lu", g_scn[i].name, per(g_scn[i].us));
    log(fd, "%s\n", line);
    for (int i = 0; i < g_scnCount; ++i)
        g_scn[i].us = 0;
    log(fd, "vita-cpu-j3d-0.1ms mat_load=%lu shape_mtx=%lu shape_dl=%lu mat_packets=%lu shape_packets=%lu\n",
        per(g_slots[0]), per(g_slots[1]), per(g_slots[2]), static_cast<unsigned long>(g_slots[3] / frames),
        static_cast<unsigned long>(g_slots[4] / frames));
    std::memset(g_slots, 0, sizeof g_slots);
    {
        // The 12 costliest top-level packet kinds (vtable addresses; the ELF names them).
        bool used[64]{};
        n = std::snprintf(line, sizeof line, "vita-cpu-packets-0.01ms");
        for (int k = 0; k < 12; ++k) {
            int best = -1;
            for (int i = 0; i < 64; ++i)
                if (!used[i] && g_vt[i].vt && (best < 0 || g_vt[i].us > g_vt[best].us))
                    best = i;
            if (best < 0)
                break;
            used[best] = true;
            n += std::snprintf(line + n, sizeof line - n, " %08lx=%lu/%u",
                               static_cast<unsigned long>(reinterpret_cast<uintptr_t>(g_vt[best].vt)),
                               static_cast<unsigned long>(g_vt[best].us / frames / 10), (g_vt[best].count + frames / 2) / frames);
        }
        log(fd, "%s\n", line);
        std::memset(g_vt, 0, sizeof g_vt);
    }
    std::memset(g_profProc, 0, sizeof g_profProc);
    std::memset(g_profCount, 0, sizeof g_profCount);
    std::memset(g_profSection, 0, sizeof g_profSection);
    for (int i = 0; i < g_bucketCount; ++i)
        g_buckets[i].us = 0;
}
void pc_gpu_group_packet(const void *) {}
// Per-phase times for the periodic report in vita_gxm_frames.cpp.
static uint64_t perf_start_us[PC_PERF_PHASES];
static uint64_t perf_total_us[PC_PERF_PHASES];

void tww_vita_perf_totals(unsigned long long *out, unsigned int count)
{
    for (unsigned int i = 0; i < count && i < PC_PERF_PHASES; ++i)
        out[i] = perf_total_us[i];
}

void pc_perf_begin(int phase)
{
    if (phase >= 0 && phase < PC_PERF_PHASES)
        perf_start_us[phase] = sceKernelGetProcessTimeWide();
    if (phase == PC_PERF_PAINTER)
        aurora_vita_set_worker_prof(tww_vita_cpu_profile);
    if (phase == PC_PERF_PAINTER && tww_vita_cpu_profile) {
        g_bucketName = "pre";
        g_bucketStart = perf_start_us[phase];
    }
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
    if (phase >= 0 && phase < PC_PERF_PHASES && perf_start_us[phase] != 0)
        perf_total_us[phase] += sceKernelGetProcessTimeWide() - perf_start_us[phase];
    if (phase == PC_PERF_PAINTER && g_bucketName) {
        bucket_close(sceKernelGetProcessTimeWide());
        g_bucketName = nullptr;
    }
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

// Button icons in message text (the 01.44 test showed gaps where they belong):
// the textures each icon picture ends up with, once per icon number.
#include "global.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/JUTTexture.h"

extern "C" void tww_vita_icon_log(int icon, const char *name, J2DPicture *pic)
{
    static uint64_t logged[2];
    if (icon < 0 || icon >= 128 || (logged[icon / 64] & (1ull << (icon % 64))))
        return;
    logged[icon / 64] |= 1ull << (icon % 64);
    pc::writef(2, "vita-icon %d name=%s shown=%d white=%08x\n", icon, name ? name : "(null)",
               pic && pic->isVisible() ? 1 : 0, pic ? static_cast<unsigned>(u32(pic->getWhite())) : 0u);
    for (u8 i = 0; pic && i < 4; ++i) {
        const JUTTexture *texture = pic->getTexture(i);
        if (!texture)
            break;
        const ResTIMG *info = texture->getTexInfo();
        if (!info) {
            pc::writef(2, "vita-icon %d tex%u no info\n", icon, i);
            continue;
        }
        pc::writef(2, "vita-icon %d tex%u fmt=%u %ux%u alpha=%u ci=%u tlut=%u colors=%u mips=%u\n", icon, i,
                   info->format, unsigned(u16(info->width)), unsigned(u16(info->height)), info->alphaEnabled,
                   info->indexTexture, info->colorFormat, unsigned(u16(info->numColors)), info->mipmapCount);
    }
}

// What a message icon picture holds when it is drawn, once per icon number;
// it also arms the renderer's trace of the draws that follow.
extern "C" void aurora_vita_arm_icon_trace();
extern "C" void tww_vita_icon_draw_log(int icon, J2DPicture *pic, int x, int y, int size)
{
    static uint64_t logged[2];
    if (!pic || icon < 0 || icon >= 128 || (logged[icon / 64] & (1ull << (icon % 64))))
        return;
    logged[icon / 64] |= 1ull << (icon % 64);
    pc::writef(2, "vita-icon-draw %d textures=%u konst=%08x konstA=%08x alpha=%u visible=%d at %d,%d size %d white=%08x black=%08x\n",
               icon, unsigned(pic->vitaNumTexture()), unsigned(pic->vitaBlendKonst()), unsigned(pic->vitaBlendKonstAlpha()),
               unsigned(pic->vitaAlpha()), pic->isVisible() ? 1 : 0, x, y, size, unsigned(u32(pic->getWhite())),
               unsigned(u32(pic->getBlack())));
    aurora_vita_arm_icon_trace();
}
