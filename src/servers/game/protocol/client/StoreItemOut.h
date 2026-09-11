#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_STORE_ITEM_OUT (wire code 411058, c2s) -- withdraw an item from the
// bank into the inventory.
struct StoreItemOut : ClientProtocol
{
    // Wire-relative, same convention as ItemPickup::slot_id.
    std::uint32_t inventory_slot_id = 0;
    // 0-79, indexes bank_items.slot_id directly (see
    // protocol/server/StoreOpenSucc.h::kSlotCount).
    std::uint32_t bank_slot_id = 0;
    // Plain unit count -- 1 means 1 item (unlike the wire's
    // quantity-1 dual-purpose convention used elsewhere).
    std::uint32_t amount = 0;
    std::uint32_t unknown = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
