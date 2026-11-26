// Settings menu (D-pad right + Square), as in sms-vita. It owns
// ux0:data/tww/settings.txt: read at start-up, written on every change.
// The menu takes the pad while open and is drawn by the GXM display callback
// into each finished frame, so it costs the game and GPU nothing.

#include <dolphin/gx/GXAurora.h>
#include <cstdlib>
#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
extern volatile int aurora_vita_show_fps;
extern void (*volatile aurora_vita_overlay_hook)(void* address, unsigned stride, unsigned width, unsigned height);
void aurora_vita_set_efb_scale(unsigned percent);
void aurora_vita_apply_efb_scale(unsigned percent);
void aurora_vita_set_float_tev(int enabled);
void aurora_vita_set_gx_thread(int enabled);
extern int tww_vita_fast_grass_enabled; // d_grass.cpp (switchwaker-vita-fast-grass.patch)
unsigned aurora_vita_efb_width();
unsigned aurora_vita_efb_height();
void pc_milestone(const char* name);
}
namespace pc {
void writef(int, const char* format, ...);
}

namespace {
constexpr char kFile[] = "ux0:data/tww/settings.txt";

volatile int g_showFps = 1;
volatile int g_resolution = 0;   // percent of the GX frame in each dimension (live); 0: AUTO
volatile int g_autoPercent = 100; // AUTO's current choice
volatile int g_floatTev = 1;     // read at start-up
volatile int g_aspect = 1;       // read at start-up: 0 4:3, 1 16:9
volatile int g_gxThread = 1;     // read at start-up: 0 off, 1 on, 2 full
int g_appliedResolution = -1;

// RESOLUTION AUTO: lowers the render size only while the game waits for the
// GPU (the frame admission wait), as sms-vita's AUTO does; raises it again
// when the GPU has had time to spare for a while.
constexpr int kAutoSteps[] = {100, 87, 75, 62, 50};
struct AutoRes {
    uint64_t waitUs = 0, frameUs = 0, windowStart = 0;
    int calmWindows = 0, busyWindows = 0;
    bool settle = false;
    // A size AUTO stepped down from stays out of reach for a while: 01.47
    // went back and forth between 62% and 75% every few seconds, and each
    // change rebuilds the render targets.
    int blockedPercent = 0;
    uint64_t blockedUntil = 0;
} g_auto;

enum Shown { ON_OFF, PERCENT, GX_MODE, ASPECT };
struct Item {
    const char* key;
    const char* label;
    volatile int* value;
    const int* choices;
    int count;
    Shown shown;
    int def;
};
const int kOnOff[] = {1, 0};
const int kGxThread[] = {0, 1, 2};
const int kScales[] = {0, 100, 87, 75, 62, 50, 37};
const Item kItems[] = {
    {"show_fps", "SHOW FPS", &g_showFps, kOnOff, 2, ON_OFF, 1},
    {"resolution", "RESOLUTION", &g_resolution, kScales, 7, PERCENT, 0},
    {"float_tev", "FAST COLOUR MIXING (NEXT START)", &g_floatTev, kOnOff, 2, ON_OFF, 1},
    {"aspect", "SCREEN (NEXT START)", &g_aspect, kOnOff, 2, ASPECT, 1},
    {"fast_grass", "FAST GRASS", &tww_vita_fast_grass_enabled, kOnOff, 2, ON_OFF, 1},
    {"gx_thread", "GX THREAD (NEXT START)", &g_gxThread, kGxThread, 3, GX_MODE, 1},
};
constexpr int kCount = int(sizeof kItems / sizeof kItems[0]);

volatile bool g_open;
volatile int g_selected;
uint32_t g_prev;
bool g_waitRelease;  // after closing: buttons stay away from the game until let go

void save()
{
    const SceUID file = sceIoOpen(kFile, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (file < 0)
        return;
    char text[512];
    int length = std::snprintf(text, sizeof text,
        "# SwitchWaker Vita settings. The in-game menu (D-pad right + Square) writes this file.\n");
    for (const Item& it : kItems)
        length += std::snprintf(text + length, sizeof text - length, "%s=%d\n", it.key, *it.value);
    sceIoWrite(file, text, static_cast<SceSize>(length));
    sceIoClose(file);
}

void step(const Item& it, int dir)
{
    int i = 0;
    while (i < it.count && it.choices[i] != *it.value)
        ++i;
    if (i == it.count)
        i = 0;
    i = (i + dir + it.count) % it.count;
    *it.value = it.choices[i];
    pc::writef(2, "vita-menu %s=%d\n", it.key, *it.value);
    if (it.value == &g_showFps)
        aurora_vita_show_fps = g_showFps;
    save();
}

// ---- drawing: a 5x7 font, scaled 2x, straight into the display buffer
struct Glyph {
    char c;
    uint8_t rows[7];
};
const Glyph kFont[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}, {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}}, {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}}, {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}}, {'/', {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}},
    {':', {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}}, {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}}, {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}}, {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}}, {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}}, {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}}, {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}}, {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}}, {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}}, {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}}, {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}}, {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}}, {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}}, {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}}, {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}}, {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}}, {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}}, {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
};

const uint8_t* glyph(char c)
{
    if (c >= 'a' && c <= 'z')
        c = char(c - 'a' + 'A');
    for (const Glyph& g : kFont)
        if (g.c == c)
            return g.rows;
    return kFont[0].rows;
}

constexpr int kScale = 2, kCellW = 6 * kScale, kLineH = 10 * kScale;

struct Canvas {
    volatile uint32_t* pixels;
    unsigned stride, width, height;
    uint32_t color;  // A8B8G8R8: 0xAABBGGRR

    void rect(int x, int y, int w, int h) const
    {
        const int x1 = x + w < int(width) ? x + w : int(width);
        const int y1 = y + h < int(height) ? y + h : int(height);
        for (int yy = y < 0 ? 0 : y; yy < y1; ++yy)
            for (int xx = x < 0 ? 0 : x; xx < x1; ++xx)
                pixels[unsigned(yy) * stride + unsigned(xx)] = color;
    }

    void text(int x, int y, const char* s) const
    {
        for (; *s; ++s, x += kCellW) {
            const uint8_t* g = glyph(*s);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (g[row] & (0x10 >> col))
                        rect(x + col * kScale, y + row * kScale, kScale, kScale);
        }
    }
};

// Display callback thread (core 3): reads the menu state, writes nothing back.
void draw_menu(void* address, unsigned stride, unsigned width, unsigned height)
{
    if (!g_open) {
        // Under Aurora's FPS counter: the render size in use (with AUTO, its current step).
        if (g_showFps) {
            Canvas c{static_cast<volatile uint32_t*>(address), stride, width, height, 0};
            char res[24];
            const int percent = g_resolution ? g_resolution : g_autoPercent;
            std::snprintf(res, sizeof res, "%d%% %uX%u", percent, aurora_vita_efb_width(), aurora_vita_efb_height());
            const int tw = int(std::strlen(res)) * kCellW + 8;
            const int x = (int(width) - tw) / 2;
            c.color = 0xff181818;
            c.rect(x, 24, tw, 18);
            c.color = g_resolution ? 0xffffffff : 0xff99e6ff;  // AUTO in yellow
            c.text(x + 4, 26, res);
        }
        return;
    }
    Canvas c{static_cast<volatile uint32_t*>(address), stride, width, height, 0};
    const int x0 = 40, y0 = 60, pad = 14, valueCol = 34 * kCellW;
    const int w = 47 * kCellW + 2 * pad, h = (kCount + 5) * kLineH + 2 * pad;
    c.color = 0xff301010;
    c.rect(x0, y0, w, h);
    c.color = 0xff1ac0f2;
    c.text(x0 + pad, y0 + pad, "SETTINGS");
    const int selected = g_selected;
    for (int i = 0; i < kCount; ++i) {
        const Item& it = kItems[i];
        const int y = y0 + pad + (i + 1) * kLineH + kLineH / 2;
        if (i == selected) {
            c.color = 0xff994d33;
            c.rect(x0 + pad / 2, y - 3, w - pad, kLineH);
        }
        c.color = 0xffffffff;
        c.text(x0 + pad, y, it.label);
        char v[16];
        if (it.shown == ON_OFF)
            std::snprintf(v, sizeof v, "%s", *it.value ? "ON" : "OFF");
        else if (it.shown == ASPECT)
            std::snprintf(v, sizeof v, "%s", *it.value ? "16:9" : "4:3");
        else if (it.shown == GX_MODE)
            std::snprintf(v, sizeof v, "%s", *it.value == 2 ? "FULL" : *it.value ? "ON" : "OFF");
        else if (*it.value == 0)
            std::snprintf(v, sizeof v, "AUTO %d%%", g_autoPercent);
        else
            std::snprintf(v, sizeof v, "%d%%", *it.value);
        c.color = *it.value != it.def ? 0xff66ff66 : 0xff999999;  // green: changed from the default
        c.text(x0 + pad + valueCol, y, v);
    }
    char info[48];
    std::snprintf(info, sizeof info, "RENDER %uX%u", aurora_vita_efb_width(), aurora_vita_efb_height());
    c.color = 0xffb3b3b3;
    c.text(x0 + pad, y0 + pad + (kCount + 2) * kLineH, info);
    c.text(x0 + pad, y0 + pad + (kCount + 3) * kLineH + kLineH / 2, "CROSS/RIGHT: CHANGE  LEFT: BACK  CIRCLE: CLOSE");
}
} // namespace

// Start-up: the saved choices, applied before Aurora creates its targets.
extern "C" void tww_vita_settings_load()
{
    const SceUID file = sceIoOpen(kFile, SCE_O_RDONLY, 0);
    if (file >= 0) {
        char text[2048];
        const int length = sceIoRead(file, text, sizeof text - 1);
        sceIoClose(file);
        text[length > 0 ? length : 0] = '\0';
        for (char* line = text; line && *line;) {
            char* next = std::strchr(line, '\n');
            if (next)
                *next++ = '\0';
            for (const Item& it : kItems) {
                const size_t n = std::strlen(it.key);
                if (std::strncmp(line, it.key, n) || line[n] != '=')
                    continue;
                int value = 0;
                if (std::sscanf(line + n + 1, "%d", &value) != 1)
                    continue;
                if (it.shown == PERCENT)  // 0 (AUTO) or any percent from a hand-edited file
                    *it.value = value <= 0 ? 0 : value < 25 ? 25 : value > 100 ? 100 : value;
                else {
                    // Only a listed choice (GX THREAD has 0/1/2); anything else keeps the default.
                    for (int k = 0; k < it.count; ++k)
                        if (it.choices[k] == value)
                            *it.value = value;
                }
            }
            line = next;
        }
    }
    save();
    aurora_vita_show_fps = g_showFps;
    aurora_vita_set_efb_scale(unsigned(g_resolution ? g_resolution : g_autoPercent));
    aurora_vita_set_float_tev(g_floatTev);
    aurora_vita_set_gx_thread(g_gxThread);
    g_appliedResolution = g_resolution ? g_resolution : g_autoPercent;
    aurora_vita_overlay_hook = draw_menu;
    pc::writef(2, "vita-settings resolution=%d aspect=%s float_tev=%d gx_thread=%d show_fps=%d\n", g_resolution, g_aspect ? "16:9" : "4:3", g_floatTev, g_gxThread, g_showFps);
}

// Each frame's wait for a free GPU slot (game thread, after admission).
extern "C" void tww_vita_autores_note(uint64_t now_us, uint64_t wait_us)
{
    static uint64_t last;
    if (last && now_us > last) {
        g_auto.frameUs += now_us - last;
        g_auto.waitUs += wait_us;
    }
    last = now_us;
    if (!g_auto.windowStart)
        g_auto.windowStart = now_us;
    if (now_us - g_auto.windowStart < 1500000 || g_resolution != 0)
        return;
    const uint64_t wait = g_auto.waitUs, frame = g_auto.frameUs;
    g_auto.waitUs = g_auto.frameUs = 0;
    g_auto.windowStart = now_us;
    if (g_auto.settle) {  // the window after a change measures the old size too
        g_auto.settle = false;
        return;
    }
    int step = 0;
    while (step < 4 && kAutoSteps[step] > g_autoPercent)
        ++step;
    const bool busy = frame && wait * 100 > frame * 12;
    const bool veryBusy = frame && wait * 100 > frame * 30;
    if (busy && step < 4) {
        // Two busy windows in a row (or one very busy one): smaller.
        if (veryBusy || ++g_auto.busyWindows >= 2) {
            g_auto.blockedPercent = g_autoPercent;
            g_auto.blockedUntil = now_us + 20000000;
            g_autoPercent = kAutoSteps[step + 1];
            g_auto.busyWindows = 0;
            g_auto.settle = true;
        }
        g_auto.calmWindows = 0;
    } else if (frame && wait * 100 < frame * 2) {
        g_auto.busyWindows = 0;
        const bool blocked = step > 0 && kAutoSteps[step - 1] >= g_auto.blockedPercent && now_us < g_auto.blockedUntil;
        if (++g_auto.calmWindows >= 4 && step > 0 && !blocked) {  // ~6 s with the GPU idle: larger
            g_autoPercent = kAutoSteps[step - 1];
            g_auto.calmWindows = 0;
            g_auto.settle = true;
        }
    } else {
        g_auto.calmWindows = 0;
        g_auto.busyWindows = 0;
    }
}

// Between frames (game thread): a new resolution rebuilds the render targets.
// 16:9: SwitchWaker's widescreen code (COS_ASPECT, pc_aspect.h) draws an anamorphic
// picture into the 640x480 EFB; Aurora's 16:9 policy renders it into a 16:9 target
// and presents it across the whole screen.
extern "C" void tww_vita_aspect_setup()
{
    setenv("COS_ASPECT", g_aspect ? "16:9" : "4:3", 1);
}

extern "C" void tww_vita_settings_frame()
{
    static bool policySet;
    if (!policySet) {
        policySet = true;
        if (g_aspect)
            AuroraSetViewportPolicy(static_cast<AuroraViewportPolicy>(3)); // mh3 Aurora: AURORA_VIEWPORT_16_9
    }
    const int resolution = g_resolution ? g_resolution : g_autoPercent;
    if (resolution == g_appliedResolution)
        return;
    g_appliedResolution = resolution;
    aurora_vita_apply_efb_scale(unsigned(resolution));
    pc::writef(2, "vita-resolution %d%% render=%ux%u\n", resolution, aurora_vita_efb_width(), aurora_vita_efb_height());
}

// Every pad read: true while the menu has the buttons (the game then sees none).
extern "C" bool tww_vita_menu_pad(uint32_t buttons)
{
    const uint32_t pressed = buttons & ~g_prev;
    g_prev = buttons;
    if (!g_open) {
        if (g_waitRelease) {
            if (buttons & (SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE | SCE_CTRL_LEFT | SCE_CTRL_RIGHT | SCE_CTRL_CROSS))
                return true;
            g_waitRelease = false;
        }
        if ((buttons & SCE_CTRL_RIGHT) && (buttons & SCE_CTRL_SQUARE) && (pressed & (SCE_CTRL_RIGHT | SCE_CTRL_SQUARE))) {
            g_open = true;
            pc_milestone("vita-menu-open");
            return true;
        }
        return false;
    }
    if (pressed & SCE_CTRL_CIRCLE) {
        g_open = false;
        g_waitRelease = true;
        return true;
    }
    int selected = g_selected;
    if (pressed & SCE_CTRL_UP)
        selected = (selected + kCount - 1) % kCount;
    if (pressed & SCE_CTRL_DOWN)
        selected = (selected + 1) % kCount;
    g_selected = selected;
    if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_RIGHT))
        step(kItems[selected], 1);
    if (pressed & SCE_CTRL_LEFT)
        step(kItems[selected], -1);
    return true;
}
