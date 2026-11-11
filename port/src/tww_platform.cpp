// Built with the platform layer (the Sunshine decomp's SDK headers): the SDK
// functions the Wind Waker game calls that the Sunshine port's platform
// layer does not provide, and the glue between the two.
#include "port_compat.h"
#include "port_platform.h"
#include "port_os.h"
#include "endian/port_endian.h"
#include <dolphin/os.h>
#include <dolphin/os/OSAlarm.h>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <dolphin/vi.h>
#include <pthread.h>
#include <time.h>
#include <cstring>
#include <strings.h>
#include <vector>

extern "C" void port_log(const char* fmt, ...);

// The window's title (sms_gx).
extern "C" const char* port_window_title = "The Legend of Zelda: The Wind Waker";

// --- The game's entry point -------------------------------------------------
// m_Do_main.cpp's `int main(int, const char**)` (renamed SMS_main by
// port_compat.h); the Sunshine port's start-up (port_main.cpp) calls
// SMS_main(void).
int SMS_main(int argc, const char* argv[]);
void SMS_main(void)
{
	static const char* argv[] = {"tww", NULL};
	SMS_main(1, argv);
}

// --- Alarms (OSAlarm) -------------------------------------------------------
// Sunshine sets none; Wind Waker sleeps its display thread on one each frame
// (JFWDisplay::threadSleep), times out GPU waits and polls the disc drive.
// The armed alarms are kept in a list; a host thread waits for the earliest
// and raises an "interrupt" (port_irq_kick), and the handlers run at the
// next interrupt check point, in interrupt context, as on the console.
namespace {
pthread_mutex_t s_alarm_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_alarm_cv = PTHREAD_COND_INITIALIZER;
std::vector<OSAlarm*> s_alarms;
bool s_alarm_thread_started = false;

// The clock without OSGetTime's interrupt check: the alarm thread asks while
// holding s_alarm_lock, and the check would run alarm_poll, which takes it
// (a deadlock: the game froze at the first frame's alarm).
s64 alarm_now() { return port_time_ticks(); }

void deadline_in(s64 ticks, struct timespec* ts)
{
	clock_gettime(CLOCK_REALTIME, ts);
	if (ticks < 0)
		ticks = 0;
	const double s = (double)ticks / (double)OS_TIMER_CLOCK;
	long long ns = ts->tv_nsec + (long long)(s * 1e9);
	ts->tv_sec += (time_t)(ns / 1000000000LL);
	ts->tv_nsec = (long)(ns % 1000000000LL);
}

void* alarm_thread(void*)
{
	pthread_mutex_lock(&s_alarm_lock);
	for (;;) {
		if (s_alarms.empty()) {
			pthread_cond_wait(&s_alarm_cv, &s_alarm_lock);
			continue;
		}
		s64 first = s_alarms[0]->fire;
		for (size_t i = 1; i < s_alarms.size(); i++)
			if (s_alarms[i]->fire < first)
				first = s_alarms[i]->fire;
		const s64 wait = first - alarm_now();
		struct timespec ts;
		if (wait <= 0) {
			pthread_mutex_unlock(&s_alarm_lock);
			port_irq_kick();
			// (the check point takes the due ones off the list; look again soon)
			deadline_in(OS_TIMER_CLOCK / 2000, &ts);
			pthread_mutex_lock(&s_alarm_lock);
		} else {
			deadline_in(wait, &ts);
		}
		pthread_cond_timedwait(&s_alarm_cv, &s_alarm_lock, &ts);
	}
	return NULL;
}

// Interrupt source (port_irq_add_source): runs the handlers of the alarms
// that are due, in interrupt context.
void alarm_poll()
{
	for (;;) {
		OSAlarm* due = NULL;
		const s64 now = alarm_now();
		pthread_mutex_lock(&s_alarm_lock);
		for (size_t i = 0; i < s_alarms.size(); i++) {
			if (s_alarms[i]->fire <= now) {
				due = s_alarms[i];
				if (due->period > 0) {
					const s64 n = (now - due->start) / due->period + 1;
					due->fire = due->start + n * due->period;
				} else {
					s_alarms.erase(s_alarms.begin() + i);
				}
				break;
			}
		}
		pthread_mutex_unlock(&s_alarm_lock);
		if (!due)
			return;
		OSAlarmHandler h = due->handler;
		if (due->period <= 0)
			due->handler = NULL;
		if (h)
			h(due, NULL);
	}
}

void arm(OSAlarm* alarm)
{
	pthread_mutex_lock(&s_alarm_lock);
	if (!s_alarm_thread_started) {
		s_alarm_thread_started = true;
		port_irq_add_source(alarm_poll);
		pthread_t th;
		pthread_create(&th, NULL, alarm_thread, NULL);
		pthread_detach(th);
	}
	bool listed = false;
	for (size_t i = 0; i < s_alarms.size(); i++)
		listed |= s_alarms[i] == alarm;
	if (!listed)
		s_alarms.push_back(alarm);
	pthread_cond_signal(&s_alarm_cv);
	pthread_mutex_unlock(&s_alarm_lock);
}
} // namespace

extern "C" void OSCreateAlarm(OSAlarm* alarm)
{
	alarm->handler = NULL;
	alarm->tag = 0;
}

extern "C" void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler)
{
	alarm->handler = handler;
	alarm->period = 0;
	alarm->start = 0;
	alarm->fire = alarm_now() + tick;
	arm(alarm);
}

extern "C" void OSSetAbsAlarm(OSAlarm* alarm, long long time, OSAlarmHandler handler)
{
	alarm->handler = handler;
	alarm->period = 0;
	alarm->start = 0;
	alarm->fire = time;
	arm(alarm);
}

extern "C" void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler)
{
	alarm->handler = handler;
	alarm->period = period;
	alarm->start = start;
	const s64 now = alarm_now();
	alarm->fire = start;
	if (period > 0 && start < now)
		alarm->fire = start + ((now - start) / period + 1) * period;
	arm(alarm);
}

extern "C" void OSCancelAlarm(OSAlarm* alarm)
{
	pthread_mutex_lock(&s_alarm_lock);
	for (size_t i = 0; i < s_alarms.size(); i++) {
		if (s_alarms[i] == alarm) {
			s_alarms.erase(s_alarms.begin() + i);
			break;
		}
	}
	alarm->handler = NULL;
	pthread_mutex_unlock(&s_alarm_lock);
}

// --- Locked cache (THP movie decoding, d_a_movie_player) --------------------
// The locked cache is ordinary memory here: a "DMA" out of it is a copy,
// finished at once.
extern "C" u32 LCStoreData(void* destAddr, void* srcAddr, u32 nBytes) { memmove(destAddr, srcAddr, nBytes); return 1; }
extern "C" u32 LCLoadData(void* destAddr, void* srcAddr, u32 nBytes) { memmove(destAddr, srcAddr, nBytes); return 1; }
extern "C" void LCQueueWait(u32 len) { (void)len; }
extern "C" u32 LCQueueLength(void) { return 0; }

// --- Game Boy Advance link (Tingle Tuner) -----------------------------------
// No Game Boy Advance is ever connected.
#define GBA_NOT_READY 1
extern "C" void GBAInit(void) {}
extern "C" s32 GBAGetStatus(s32, u8* status) { if (status) *status = 0; return GBA_NOT_READY; }
extern "C" s32 GBAGetProcessStatus(s32, u8* percentp) { if (percentp) *percentp = 0; return GBA_NOT_READY; }
extern "C" s32 GBAReset(s32, u8* status) { if (status) *status = 0; return GBA_NOT_READY; }
extern "C" s32 GBARead(s32, u8*, u8* status) { if (status) *status = 0; return GBA_NOT_READY; }
extern "C" s32 GBAWrite(s32, u8*, u8* status) { if (status) *status = 0; return GBA_NOT_READY; }
extern "C" s32 GBAJoyBoot(s32, s32, s32, u8*, u32, u8* status) { if (status) *status = 0; return GBA_NOT_READY; }
// Serial devices: only controllers (pad.cpp answers for those).
extern "C" u32 SIProbe(s32 chan) { (void)chan; return 0x09000000; /* SI_GC_CONTROLLER */ }

// --- Memory card --------------------------------------------------------------
extern "C" s32 CARDGetSerialNo(s32 chan, u64* serialNo)
{
	(void)chan;
	if (serialNo)
		*serialNo = 0x5457575649544131ull;  // any fixed number: one card
	return 0;  // CARD_RESULT_READY
}

// --- OS odds and ends ---------------------------------------------------------
extern "C" void OSSetStringTable(const void* table) { (void)table; }
extern "C" void OSSetSaveRegion(void* start, void* end) { (void)start; (void)end; }
extern "C" s32 OSCheckActiveThreads(void) { return 1; }
extern "C" void OSClearContext(OSContext* context) { (void)context; }
extern "C" void OSSetCurrentContext(OSContext* context) { (void)context; }
static u32 s_fake_stack_word;
extern "C" u32 OSGetStackPointer(void) { return (u32)(uintptr_t)&s_fake_stack_word; }
extern "C" { u32 __OSFpscrEnableBits = 0; }
extern "C" void PPCHalt(void) { for (;;) { struct timespec ts = {1, 0}; nanosleep(&ts, NULL); } }
extern "C" u32 PPCMfhid2(void) { return 0; }
extern "C" void PPCMtmsr(u32 value) { (void)value; }

// --- Render modes ---------------------------------------------------------------
// NTSC 640x480 interlaced with the deflicker filter (the SDK's documented
// values; only the vertical filter differs from GXNtsc480Int).
GXRenderModeObj GXNtsc480IntDf = {
	VI_TVMODE_NTSC_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, GX_FALSE, GX_FALSE,
	{ { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 }, { 6, 6 } },
	{ 8, 8, 10, 12, 10, 8, 8 },
};

// --- Matrix and vector library: the C versions the game calls by name -----------
extern "C" void C_VECHalfAngle(Vec* a, Vec* b, Vec* half)
{
	Vec na = {-a->x, -a->y, -a->z}, nb = {-b->x, -b->y, -b->z};
	PSVECNormalize(&na, &na);
	PSVECNormalize(&nb, &nb);
	Vec h = {na.x + nb.x, na.y + nb.y, na.z + nb.z};
	if (PSVECDotProduct(&h, &h) > 0.0f)
		PSVECNormalize(&h, half);
	else
		*half = h;
}

extern "C" void C_VECReflect(Vec* src, Vec* normal, Vec* dst)
{
	Vec ns = {-src->x, -src->y, -src->z}, nn;
	PSVECNormalize(&ns, &ns);
	PSVECNormalize(normal, &nn);
	const f32 cosA = PSVECDotProduct(&ns, &nn);
	Vec r = {2.0f * nn.x * cosA - ns.x, 2.0f * nn.y * cosA - ns.y, 2.0f * nn.z * cosA - ns.z};
	PSVECNormalize(&r, dst);
}

extern "C" void C_QUATMultiply(const Quaternion* p, const Quaternion* q, Quaternion* pq)
{
	Quaternion r;
	r.x = p->w * q->x + p->x * q->w + p->y * q->z - p->z * q->y;
	r.y = p->w * q->y + p->y * q->w + p->z * q->x - p->x * q->z;
	r.z = p->w * q->z + p->z * q->w + p->x * q->y - p->y * q->x;
	r.w = p->w * q->w - p->x * q->x - p->y * q->y - p->z * q->z;
	*pq = r;
}

extern "C" void C_QUATNormalize(const Quaternion* src, Quaternion* unit)
{
	f32 mag = src->x * src->x + src->y * src->y + src->z * src->z + src->w * src->w;
	if (mag >= 0.00001f) {
		mag = 1.0f / sqrtf(mag);
		unit->x = src->x * mag;
		unit->y = src->y * mag;
		unit->z = src->z * mag;
		unit->w = src->w * mag;
	} else {
		unit->x = unit->y = unit->z = unit->w = 0.0f;
	}
}

extern "C" void C_QUATRotAxisRad(Quaternion* q, const Vec* axis, f32 rad)
{
	Vec dst;
	PSVECNormalize((Vec*)axis, &dst);
	const f32 half = rad * 0.5f;
	const f32 s = sinf(half);
	q->x = s * dst.x;
	q->y = s * dst.y;
	q->z = s * dst.z;
	q->w = cosf(half);
}

extern "C" void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t)
{
	f32 cosTheta = p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;
	f32 sign = 1.0f;
	if (cosTheta < 0.0f) {
		cosTheta = -cosTheta;
		sign = -1.0f;
	}
	f32 tp, tq;
	if (cosTheta <= 0.99999f) {
		const f32 theta = acosf(cosTheta);
		const f32 sinTheta = sinf(theta);
		tp = sinf((1.0f - t) * theta) / sinTheta;
		tq = sinf(t * theta) / sinTheta;
	} else {
		tp = 1.0f - t;
		tq = t;
	}
	tq *= sign;
	r->x = tp * p->x + tq * q->x;
	r->y = tp * p->y + tq * q->y;
	r->z = tp * p->z + tq * q->z;
	r->w = tp * p->w + tq * q->w;
}

// --- Sunshine-only parts of the platform layer, not built here ------------------
// The warp menu (platform/misc/port_warp.cpp; the graphics layer draws it).
extern "C" int port_warp_ready(void) { return 0; }
extern "C" int port_warp_count(void) { return 0; }
extern "C" const void* port_warp_entry(int) { return NULL; }
extern "C" void port_warp_request(int) {}
extern "C" int port_warp_want_list(void) { return 0; }
// Sunshine's own file formats' byte order (platform/endian/endian_game.cpp):
// Wind Waker's are added as the game reads them.
// Wind Waker's own formats, by name (port/src/tww_endian.cpp). Returns a
// PortEndianFormat: PE_FMT_GAME when converted, PE_FMT_UNKNOWN otherwise.
extern "C" int tww_endian_stage(void* data, uint32_t size, const char* name);
extern "C" int tww_endian_event_list(void* data, uint32_t size);
static bool ends_with(const char* s, const char* suffix)
{
	if (!s)
		return false;
	size_t n = strlen(s), m = strlen(suffix);
	return n >= m && strcasecmp(s + n - m, suffix) == 0;
}
extern "C" int port_endian_game(void* data, uint32_t size, const char* name)
{
	const int PE_FMT_UNKNOWN_ = PE_FMT_UNKNOWN, PE_FMT_GAME_ = PE_FMT_GAME;
	if (ends_with(name, ".dzs") || ends_with(name, ".dzr"))
		return tww_endian_stage(data, size, name) ? PE_FMT_GAME_ : PE_FMT_UNKNOWN_;
	if (ends_with(name, "event_list.dat"))
		return tww_endian_event_list(data, size) ? PE_FMT_GAME_ : PE_FMT_UNKNOWN_;
	return PE_FMT_UNKNOWN_;
}
// Sunshine's HD cutscene pack (platform/thp).
extern "C" const char* port_hd_cutscene_directory(void) { return NULL; }
