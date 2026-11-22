#include "disc.h"
#include "gpu.h"
#include "JSystem/JUtility/JUTGamePad.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>

namespace {
JUTGamePad& gamepad()
{
    static JUTGamePad pad(JUTGamePad::EPort1);
    return pad;
}

struct ButtonMapping {
    unsigned int vita;
    u16 gamecube;
};

constexpr ButtonMapping buttons[] = {
    {SCE_CTRL_UP, PAD_BUTTON_UP},
    {SCE_CTRL_DOWN, PAD_BUTTON_DOWN},
    {SCE_CTRL_LEFT, PAD_BUTTON_LEFT},
    {SCE_CTRL_RIGHT, PAD_BUTTON_RIGHT},
    {SCE_CTRL_CROSS, PAD_BUTTON_A},
    {SCE_CTRL_CIRCLE, PAD_BUTTON_B},
    {SCE_CTRL_SQUARE, PAD_BUTTON_X},
    {SCE_CTRL_TRIANGLE, PAD_BUTTON_Y},
    {SCE_CTRL_LTRIGGER, PAD_TRIGGER_L},
    {SCE_CTRL_RTRIGGER, PAD_TRIGGER_R},
    {SCE_CTRL_SELECT, PAD_TRIGGER_Z},
    {SCE_CTRL_START, PAD_BUTTON_START},
};

u32 seen_buttons;
float stick_min[4], stick_max[4];
}

extern "C" BOOL PADInit()
{
    return sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG) >= 0;
}

extern "C" u32 PADRead(PADStatus *status)
{
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i) {
        status[i] = {};
        status[i].err = PAD_ERR_NO_CONTROLLER;
    }
    SceCtrlData input;
    if (sceCtrlPeekBufferPositive(0, &input, 1) <= 0) {
        status[0].err = PAD_ERR_TRANSFER;
        return 0;
    }
    PADStatus &pad = status[0];
    pad.err = PAD_ERR_NONE;
    for (const auto &mapping : buttons) {
        if (input.buttons & mapping.vita)
            pad.button |= mapping.gamecube;
    }
    // Scale to the input range of PADClamp. Vita Y increases downwards.
    pad.stickX = (static_cast<int>(input.lx) - 128) * 80 / 128;
    pad.stickY = (128 - static_cast<int>(input.ly)) * 80 / 128;
    pad.substickX = (static_cast<int>(input.rx) - 128) * 65 / 128;
    pad.substickY = (128 - static_cast<int>(input.ry)) * 65 / 128;
    pad.triggerLeft = pad.button & PAD_TRIGGER_L ? 255 : 0;
    pad.triggerRight = pad.button & PAD_TRIGGER_R ? 255 : 0;
    return 0;
}

// This backend exposes a continuously polled port without motor or reset hardware.
extern "C" void PADSetSpec(u32) {}
extern "C" void PADSetAnalogMode(u32) {}
extern "C" void PADControlMotor(u32, u32) {}
extern "C" BOOL PADReset(u32) { return TRUE; }
extern "C" BOOL PADRecalibrate(u32) { return TRUE; }
extern "C" BOOL OSGetResetSwitchState() { return FALSE; }

extern "C" void pc_vibration_guard(int) {}

extern "C" int boot_gamepad_init(SceUID log)
{
    if (!JUTGamePad::init())
        return -1;
    auto &pad = gamepad();
    for (int attempt = 0; attempt < 6; ++attempt) {
        JUTGamePad::read();
        if (pad.getErrorStatus() == PAD_ERR_NONE)
            break;
        sceKernelDelayThread(16667);
    }
    probe_log(log, "pad: JUTGamePad port 1 status %d\n", static_cast<int>(pad.getErrorStatus()));
    return pad.getErrorStatus() == PAD_ERR_NONE ? 0 : -1;
}

extern "C" int boot_gamepad_poll(SceUID log)
{
    auto &pad = gamepad();
    JUTGamePad::read();
    if (pad.getErrorStatus() != PAD_ERR_NONE)
        return 0;
    seen_buttons |= pad.getButton() & 0xffff;
    const float axes[] = {pad.getMainStickX(), pad.getMainStickY(),
                          pad.getSubStickX(), pad.getSubStickY()};
    for (int i = 0; i < 4; ++i) {
        if (axes[i] < stick_min[i])
            stick_min[i] = axes[i];
        if (axes[i] > stick_max[i])
            stick_max[i] = axes[i];
    }
    if (!(pad.getButton() & PAD_BUTTON_START))
        return 0;
    probe_log(log, "pad: JUTGamePad START, button mask 0x%04x\n", seen_buttons);
    probe_log(log, "pad: stick ranges LX %+.2f/%+.2f LY %+.2f/%+.2f RX %+.2f/%+.2f RY %+.2f/%+.2f\n",
        stick_min[0], stick_max[0], stick_min[1], stick_max[1],
        stick_min[2], stick_max[2], stick_min[3], stick_max[3]);
    return 1;
}
