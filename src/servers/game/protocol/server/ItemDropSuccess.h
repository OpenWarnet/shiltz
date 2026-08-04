#pragma once

#include <cstdint>

class PayloadWriter;

// GC_ITEM_DROP_SUCC (wire code 521034, s2c) -- acknowledges a successful
// CG_ITEM_DROP. 9 x u32 body: id (the new ground item's instance id), x, y
// (drop location -- currently always 0,0, see HandleItemDrop), item_id,
// source_slot_id (the inventory slot dropped from), then 4 reserved/unused
// zero dwords.
struct ItemDropSuccess
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t item_id = 0;
    std::uint32_t source_slot_id = 0;

    void Serialize(PayloadWriter& writer) const;
};
