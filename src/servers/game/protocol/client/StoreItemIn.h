#pragma once

#include <cstdint>

class PayloadReader;

// CG_STORE_ITEM_IN (wire code 411057, c2s) -- deposit an item from the
// inventory into the bank.
struct StoreItemIn
{
    // Wire-relative, same convention as ItemPickup::slot_id.
    std::uint32_t inventory_slot_id = 0;
    // 0-79, indexes bank_items.slot_id directly (see
    // protocol/server/StoreOpenSucc.h::kSlotCount).
    std::uint32_t bank_slot_id = 0;
    // Plain unit count -- 1 means 1 item (unlike the wire's
    // quantity-1 dual-purpose convention used elsewhere).
    std::uint32_t amount = 0;

    bool Deserialize(PayloadReader& reader);
};
