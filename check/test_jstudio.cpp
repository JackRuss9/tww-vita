// Synthetic STB data-ID payload: no game data or display needed.
#include "JSystem/JStudio/JStudio/stb-data-parse.h"
#include "JSystem/JMessage/data.h"
#include <assert.h>
#include <stdio.h>
#ifdef NDEBUG
#error JStudio checks require assertions enabled
#endif
#undef main
int main() {
    using namespace JStudio::stb;
    using namespace JGadget::binary;
    // A four-byte ID followed by three unaligned BE u16s and two hand bytes.
    const u8 bytes[] = {0,0,0,4, 0,0,0,1,
                       0x3a,3, 0x12,0x34, 0x02,0x6d, 0x02,0x1e,
                       0x39,2, 5,7, 0};
    data::TParse_TParagraph_dataID header(bytes);
    assert(header.get_IDSize() == 4);
    assert(header.getContent() == bytes + 8);
    assert(beLoad<u32>(header.get_ID()) == 1);
    data::TParse_TParagraph_data::TData anim;
    data::TParse_TParagraph_data(header.getContent()).getData(&anim);
    assert(anim.status == 50 && anim.entryCount == 3 && anim.entrySize == 2);
    TValueIterator_misaligned<u16> values(anim.content);
    assert(*values == 0x1234);
    assert(*(values + 1) == 0x026d);
    assert(*(values + 2) == 0x021e);
    assert(anim.next == bytes + 16);
    data::TParse_TParagraph_data::TData hands;
    data::TParse_TParagraph_data(anim.next).getData(&hands);
    assert(hands.status == 49 && hands.entryCount == 2 && hands.entrySize == 1);
    TValueIterator_raw<u8> hand_values(hands.content);
    assert(*hand_values == 5 && *(hand_values + 1) == 7);
    // An ID not divisible by four must still skip its alignment padding.
    const u8 padded[] = {0,0,0,1, 'X',0,0,0, 0};
    data::TParse_TParagraph_dataID short_id(padded);
    assert(short_id.get_IDSize() == 1 && short_id.getContent() == padded + 8);
    const u8 bmg_header[] = {'M','E','S','G','b','m','g','1',
                             0,0,0,64, 0,0,0,3, 1,0,0,0,
                             0,0,0,0, 0,0,0,0, 0,0,0,0};
    JMessage::data::TParse_THeader bmg(bmg_header);
    assert(bmg.get_type() == 0x626d6731 && bmg.get_blockNumber() == 3);
    assert(bmg.get_encoding() == 1);
    const u8 inf[] = {'I','N','F','1', 0,0,0,64,
                      0x01,0x23, 0,24, 0,2, 0,0};
    JMessage::data::TParse_TBlock_info info(inf);
    assert(info.get_type() == 0x494e4631 && info.get_size() == 64);
    assert(info.get_messageEntryNumber() == 0x123);
    assert(info.get_messageEntrySize() == 24 && info.get_groupID() == 2);
    puts("JStudio data-ID/animation/hand and JMessage BMG header checks passed");
}
