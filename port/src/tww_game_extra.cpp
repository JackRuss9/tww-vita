// Built with the game's code (its headers and flags): definitions the
// game refers to that the decomp does not have.
#include "JAZelAudio/JAIZelParam.h"

namespace JAIZelParam {
// Declared but never defined in the decomp (only an assertion names it).
int SEA_START_VS_ROOM_TBL_SIZE;
}

#include "f_pc/f_pc_name.h"
#include "dolphin/os/OS.h"

bool tww_actor_creation_missing(s16 profile) {
    switch (profile) {
#include "tww_missing_actors.inc"
        {
            static bool reported[0x200];
            if ((unsigned)profile < 0x200 && !reported[profile]) {
                reported[profile] = true;
                OSReport("[tww] skipping actor %d: creation not decompiled\n", profile);
            }
            return true;
        }
    default:
        return false;
    }
}
