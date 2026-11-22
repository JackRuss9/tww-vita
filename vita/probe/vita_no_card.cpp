#include <dolphin/card.h>

// The first boot configuration exposes both slots as empty. The game's own
// memory-card menu and error handling decide how to proceed in that case.
extern "C" {
void CARDInit(const char *, const char *) {}

int CARDProbe(s32) { return FALSE; }
s32 CARDProbeEx(s32, s32 *, s32 *) { return CARD_RESULT_NOCARD; }
s32 CARDMount(s32, void *, CARDCallback) { return CARD_RESULT_NOCARD; }
s32 CARDUnmount(s32) { return CARD_RESULT_NOCARD; }
s32 CARDCheck(s32) { return CARD_RESULT_NOCARD; }
s32 CARDFormat(s32) { return CARD_RESULT_NOCARD; }
s32 CARDFreeBlocks(s32, s32 *, s32 *) { return CARD_RESULT_NOCARD; }
s32 CARDGetSerialNo(s32, u64 *) { return CARD_RESULT_NOCARD; }

s32 CARDOpen(s32, const char *, CARDFileInfo *) { return CARD_RESULT_NOCARD; }
s32 CARDCreate(s32, const char *, u32, CARDFileInfo *) { return CARD_RESULT_NOCARD; }
s32 CARDClose(CARDFileInfo *) { return CARD_RESULT_NOCARD; }
s32 CARDRead(const CARDFileInfo *, void *, s32, s32) { return CARD_RESULT_NOCARD; }
s32 CARDWrite(const CARDFileInfo *, const void *, s32, s32) { return CARD_RESULT_NOCARD; }
s32 CARDGetStatus(s32, s32, CARDStat *) { return CARD_RESULT_NOCARD; }
s32 CARDSetStatus(s32, s32, const CARDStat *) { return CARD_RESULT_NOCARD; }
}
