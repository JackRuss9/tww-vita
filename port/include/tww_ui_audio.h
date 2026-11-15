#ifndef TWW_UI_AUDIO_H
#define TWW_UI_AUDIO_H
#include <stdint.h>

// Retail JAIZelBasic::seStart, 802A75EC..802A7700. Preserve the distinct
// cinematic page/end sounds, and the original dialogue suppression rules.
// UINT32_MAX means that this request must not start a sound.
inline uint32_t tww_ui_sound_id(uint32_t id, bool demo, bool blockedStream,
                                bool dialogueMode, bool silent, bool taktMatched) {
    switch (id) {
    case 0x080a: // TALK_START
    case 0x080c: // TALK_WIN_OPEN
    case 0x0825: // TALK_END
        return blockedStream || demo || dialogueMode || silent ? UINT32_MAX : id;
    case 0x080b: // TALK_NEXT
        if (blockedStream || silent) return UINT32_MAX;
        return demo || dialogueMode ? 0x08fd : id; // DEMO_MSG_NEXT
    case 0x080d: // TALK_WIN_CLOSE
        if (blockedStream || silent || taktMatched) return UINT32_MAX;
        return demo ? 0x08fe : id; // DEMO_MSG_END
    default:
        return id;
    }
}
#endif
