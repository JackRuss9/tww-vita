#include "tww_ui_audio.h"
#include <assert.h>
#include <stdio.h>
#ifdef NDEBUG
#error Audio routing checks require assertions enabled
#endif
int main() {
    // UI confirmation and a rupee request keep distinct IDs in every mode.
    for (unsigned flags = 0; flags < 32; ++flags) {
        bool demo=flags&1, stream=flags&2, dialogue=flags&4, silent=flags&8, takt=flags&16;
        assert(tww_ui_sound_id(0x0801,demo,stream,dialogue,silent,takt)==0x0801);
        assert(tww_ui_sound_id(0x0831,demo,stream,dialogue,silent,takt)==0x0831);
        assert(tww_ui_sound_id(0x0826,demo,stream,dialogue,silent,takt)==0x0826);
    }
    // Explicit retail cases: normal talk, cinematic talk, suppressed requests.
    assert(tww_ui_sound_id(0x080b,0,0,0,0,0)==0x080b);
    assert(tww_ui_sound_id(0x080b,1,0,0,0,0)==0x08fd);
    assert(tww_ui_sound_id(0x080b,0,0,1,0,0)==0x08fd);
    assert(tww_ui_sound_id(0x080d,1,0,0,0,0)==0x08fe);
    assert(tww_ui_sound_id(0x080d,0,0,0,0,0)==0x080d);
    assert(tww_ui_sound_id(0x080c,1,0,0,0,0)==UINT32_MAX);
    assert(tww_ui_sound_id(0x080b,1,1,0,0,0)==UINT32_MAX);
    assert(tww_ui_sound_id(0x080b,1,0,0,1,0)==UINT32_MAX);
    assert(tww_ui_sound_id(0x080d,0,0,0,0,1)==UINT32_MAX);
    assert(tww_ui_sound_id(0x0825,0,0,1,0,0)==UINT32_MAX);
    puts("PASS: normal/cinematic dialogue routing, suppression and distinct confirmation/rupee IDs");
}
