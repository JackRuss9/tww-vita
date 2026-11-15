#ifndef TWW_DSP_TABLES_H
#define TWW_DSP_TABLES_H
#include <stdint.h>
#include <stddef.h>

// The native mixer ABI consumes two signed coefficients per host u32,
// first coefficient in the high half. TWW defines native u16 arrays.
inline void tww_pack_dsp_table(uint32_t* words, const uint16_t* halves, size_t count) {
    for (size_t i = 0; i < count; ++i)
        words[i] = (uint32_t(halves[2 * i]) << 16) | halves[2 * i + 1];
}
#endif
