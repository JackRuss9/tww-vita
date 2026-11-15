#include "tww_dsp_tables.h"
#include "dsp_mixer.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#ifdef NDEBUG
#error DSP table checks require assertions enabled
#endif
namespace JASystem { namespace DSPInterface {
    extern uint16_t DSPRES_FILTER[], DSPADPCM_FILTER[];
} }

// Predictor 1 adds the previous decoded sample to each signed residual.
// No copyrighted audio: one synthetic AFC frame with hand-calculated output.
static const uint8_t frame[] = {0x01, 0x12, 0x34, 0x56, 0x70, 0xfe, 0xdc, 0xba, 0x90};
static const uint8_t* memory(uint32_t addr) { return addr < sizeof(frame) ? frame + addr : NULL; }
template<class T> static void put(uint8_t* voice, int offset, T value) {
    memcpy(voice + offset, &value, sizeof value);
}
static void render(const uint32_t* res, const uint32_t* adpcm, int16_t* left) {
    alignas(32) uint8_t voice[0x180] = {};
    put<uint16_t>(voice, 0x00, 1);
    put<uint16_t>(voice, 0x04, 0x1000);
    put<uint16_t>(voice, 0x08, 1);
    put<uint16_t>(voice, 0x10, 0x0d00);
    put<uint16_t>(voice, 0x12, 0x4000);
    put<uint16_t>(voice, 0x14, 0x4000);
    put<uint16_t>(voice, 0x100, 9);
    put<uint32_t>(voice, 0x114, 16);
    int16_t right[80];
    port_dspmix_setup(1, voice, res, adpcm, NULL);
    port_dspmix_render(left, right, 80, 0x8000);
}

int main() {
    using namespace JASystem::DSPInterface;
    uint32_t res[256], adpcm[16];
    tww_pack_dsp_table(res, DSPRES_FILTER, 256);
    tww_pack_dsp_table(adpcm, DSPADPCM_FILTER, 16);
    assert(res[0] == 0x0c3966ad && res[1] == 0x0d46ffdf);
    assert(adpcm[1] == 0x08000000 && adpcm[2] == 0x00000800);
    const int16_t expected[] = {1, 3, 6, 10, 15, 21, 28, 28, 27, 25, 22, 18, 13, 7, 0, 0};
    int16_t decoded[16];
    port_dspmix_setup(0, NULL, res, adpcm, NULL);
    port_dspmix_set_aram(memory);
    port_dspmix_decode_wave(0, 0, 16, decoded);
    assert(memcmp(decoded, expected, sizeof expected) == 0);
    int16_t correct[80], broken[80];
    render(res, adpcm, correct);
    for (int i = 0; i < 12; ++i) {
        // Unity pitch: phase zero, previous/current/next/next-next samples.
        int32_t sum = (i ? expected[i - 1] : 0) * 3129 + expected[i] * 26285
                      + expected[i + 1] * 3398 - expected[i + 2] * 33;
        assert(correct[i] == (sum >> 15));
    }
    // Reproduce the previous ABI mismatch without aliasing or alignment UB.
    // On little-endian hosts this swaps the predictor halves and corrupts AFC.
    uint32_t unadapted[16];
    memcpy(unadapted, DSPADPCM_FILTER, sizeof unadapted);
    if (unadapted[1] != adpcm[1]) {
        // Use the rendering path: the decode-only diagnostic hook falls
        // back to defaults when predictor 1's first coefficient is zero.
        render(res, unadapted, broken);
        assert(memcmp(broken, correct, 12 * sizeof(int16_t)) != 0);
        uint32_t unadapted_res[256];
        memcpy(unadapted_res, DSPRES_FILTER, sizeof unadapted_res);
        render(unadapted_res, adpcm, broken);
        assert(memcmp(broken, correct, 12 * sizeof(int16_t)) != 0);
    }
    puts("PASS: production TWW DSP tables preserve coefficient order and decode the AFC reference exactly");
}
