#include "pc/pc_harness.h"

#include <cos_dsp_extras.h>
#include <dolphin/ai.h>

namespace {
AIDCallback dma_callback;
u32 dsp_sample_rate;

[[noreturn]] void unexpected_dsp_start()
{
    pc_milestone("vita-muted-dsp-start-requested");
    pc_exit(1);
}
}

extern "C" {
DSPTaskInfo * __DSP_first_task;
DSPTaskInfo * __DSP_curr_task;

void AIInit(u8 *) {}
void AIInitDMA(uintptr_t, u32) {}
void AIStartDMA() {}
void AIResetStreamSampleCount() {}
void AISetStreamPlayState(u32) {}
void AISetStreamSampleRate(u32) {}
void AISetStreamVolLeft(u8) {}
void AISetStreamVolRight(u8) {}

AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
    const auto previous = dma_callback;
    dma_callback = callback;
    return previous;
}

void AISetDSPSampleRate(u32 rate) { dsp_sample_rate = rate; }
u32 AIGetDSPSampleRate() { return dsp_sample_rate; }
void DSPInit() {}
void DSPAssertInt() {}
u32 DSPCheckMailToDSP() { return 0; }
u32 DSPCheckMailFromDSP() { return 0; }
u32 DSPReadMailFromDSP() { return 0; }
void DSPSendMailToDSP(u32) {}
u16 COSDSPReadControlRegister() { return 0; }
void COSDSPWriteControlRegister(u16) {}
void __DSP_insert_task(DSPTaskInfo *) { unexpected_dsp_start(); }
void __DSP_boot_task(DSPTaskInfo *) { unexpected_dsp_start(); }
void __DSP_exec_task(DSPTaskInfo *, DSPTaskInfo *) { unexpected_dsp_start(); }
void __DSP_remove_task(DSPTaskInfo *) {}
}
