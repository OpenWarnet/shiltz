#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_ITEM_PICKUP_SUCC (wire code 521033, s2c) -- acknowledges a successful
// CG_ITEM_PICKUP. 8 x u32 body: id (the ground item's instance id), slot_id
// (where it landed), item_id, qty_or_refine (same dual-purpose wire field
// as InventoryItemSlot::qty_or_refine -- an item with a refine_level shows
// it as-is; a stackable item shows quantity - 1; see InventoryItemList.h),
// then 4 reserved/unused zero dwords.
struct ItemPickupSuccess : ServerMessage<GameOpcode::GC_ITEM_PICKUP_SUCC>
{
    std::uint32_t id = 0;
    std::uint32_t slot_id = 0;
    std::uint32_t item_id = 0;
    std::uint32_t qty_or_refine = 0;

    void Serialize(PayloadWriter& writer) const override;
};
