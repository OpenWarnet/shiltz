#pragma once

#include <cstdint>

class PayloadWriter;

// GC_ITEM_DROP_SUCC (wire code 521034, s2c) -- acknowledges a successful
// CG_ITEM_DROP. 9 x u32 body: id (the new ground item's instance id), x, y
// (drop location), item_id, source_slot_id (the inventory slot dropped
// from), new_item_id/new_item_count describing what's left in
// source_slot_id after the drop -- same qty_or_refine convention as
// ItemPickupSuccess (0 = 1 item, 1 = 2 items, ...); if the slot is now
// empty both are 0, which is how the client knows to clear it -- then 2
// still-reserved/unused zero dwords (2 of the original 4 turned out to be
// new_item_id/new_item_count).
struct ItemDropSuccess
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t item_id = 0;
    std::uint32_t source_slot_id = 0;
    std::uint32_t new_item_id = 0;
    std::uint32_t new_item_count = 0;

    void Serialize(PayloadWriter& writer) const;
};
