#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_STORE_ITEM_OUT (wire code 521119, s2c) -- acknowledges a successful
// CG_STORE_ITEM_OUT. The client applies this literally: the first four
// fields overwrite the inventory slot, the next four overwrite the bank
// slot, and the last updates the money display -- all clamping/quantity
// math has to be done server-side before this is built (see
// handlers/Store.cpp).
struct StoreItemOutSuccess : ServerProtocol
{
    std::uint32_t inventory_slot_id = 0;
    // 0 means "clear this slot" -- same convention as TradeSellSucc/
    // ItemDropSuccess.
    std::uint32_t inventory_item_id = 0;
    std::uint32_t inventory_qty_or_refine = 0;
    std::int64_t inventory_option_bits = 0;

    std::uint32_t bank_slot_id = 0;
    // 0 means "clear this slot" -- the bank slot was fully withdrawn.
    std::uint32_t bank_item_id = 0;
    std::uint32_t bank_qty_or_refine = 0;
    std::int64_t bank_option_bits = 0;

    // Character's wallet money after the flat 100 withdrawal fee.
    std::int64_t money = 0;

    void Serialize(PayloadWriter& writer) const override;
};
