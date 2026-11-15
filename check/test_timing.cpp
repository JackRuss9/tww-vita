// Standalone: g++ -O2 -Iport/include check/test_timing.cpp -o /tmp/tww-timing && /tmp/tww-timing
#include "tww_timing.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static float retail_ratio(uint32_t interval, uint32_t frame, int32_t phase) {
    int64_t i = interval;
    while (i < (int64_t)frame * 2)
        i += interval;
    int64_t delta = i - (int64_t)frame * 2 - phase;
    if (delta < 0) delta += interval;
    float ratio = (float)delta / (float)frame;
    return ratio > 1.0f ? 1.0f : ratio;
}
int main() {
    // Reference the retail loop where its signed additions and division are valid.
    for (uint32_t interval = 500; interval < 2000; interval += 127)
        for (uint32_t frame = interval; frame < interval * 8; frame += 113)
            for (int32_t phase = -(int32_t)interval / 4; phase < (int32_t)interval; phase += 83)
                assert(tww_combination_ratio(interval, frame, (uint32_t)phase) ==
                       retail_ratio(interval, frame, phase));
    assert(tww_combination_ratio(0, 100, 0) == 1.0f);
    assert(tww_combination_ratio(100, 0, 0) == 1.0f);
    const uint32_t values[] = {0, 1, 675675, 0x7fffffff, 0x80000000, 0xffffffff};
    for (uint32_t interval : values)
        for (uint32_t frame : values)
            for (uint32_t phase : values) {
                float ratio = tww_combination_ratio(interval, frame, phase);
                assert(isfinite(ratio) && ratio >= 0 && ratio <= 1);
            }
    puts("PASS: retail timing equivalence and bounded degenerate/wrap cases");
}
