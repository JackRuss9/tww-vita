#ifndef TWW_TIMING_H
#define TWW_TIMING_H
#include <stdint.h>

// The host may deliver retraces in a burst or pause for a debugger/driver.
// Keep JFWDisplay's blend ratio bounded without replaying tick intervals.
static inline float tww_combination_ratio(uint32_t interval, uint32_t frame,
                                          uint32_t phase) {
    if (!interval || !frame)
        return 1.0f;
    const uint64_t twice = (uint64_t)frame * 2;
    const uint64_t boundary = ((twice + interval - 1) / interval) * interval;
    int64_t delta = (int64_t)(boundary - twice) - (int32_t)phase;
    if (delta < 0)
        delta += interval;
    if (delta <= 0)
        return 0.0f;
    const float ratio = (float)delta / (float)frame;
    return ratio > 1.0f ? 1.0f : ratio;
}
#endif
