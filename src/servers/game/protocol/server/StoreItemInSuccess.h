#pragma once

#include <cstdint>

class PayloadWriter;

// GC_STORE_ITEM_IN (wire code 521118, s2c) -- acknowledges a successful
// CG_STORE_ITEM_IN. No money field -- depositing into the bank isn't
// charged (see handlers/Store.cpp).
struct StoreItemInSuccess
{
    std::uint32_t inventory_slot_id = 0;
    // 0 means "clear this slot" -- the inventory slot was fully deposited.
    std::uint32_t inventory_item_id = 0;
    std::uint32_t inventory_qty_or_refine = 0;
    std::int64_t inventory_option_bits = 0;

    std::uint32_t bank_slot_id = 0;
    std::uint32_t bank_item_id = 0;
    std::uint32_t bank_qty_or_refine = 0;
    std::int64_t bank_option_bits = 0;

    void Serialize(PayloadWriter& writer) const;
};
