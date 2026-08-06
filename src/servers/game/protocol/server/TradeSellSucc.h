#pragma once

#include <cstdint>

class PayloadWriter;

// GC_TRADE_SELL_SUCC (wire code 521054, s2c) -- acknowledges a successful
// CG_ITEM_TRADE_SELL.
struct TradeSellSucc
{
    std::uint32_t slot_id = 0;
    std::uint32_t item_id = 0;
    // Remaining quantity in slot_id after the sale (0 if the slot was
    // emptied entirely).
    std::uint32_t new_count = 0;
    std::uint32_t option = 0;
    std::uint32_t option2 = 0;
    std::int64_t money_after = 0;

    void Serialize(PayloadWriter& writer) const;
};
