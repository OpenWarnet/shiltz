#pragma once

#include <cstdint>

class PayloadWriter;

// GC_TRADE_BUY_FAIL (wire code 531053, s2c) -- rejects CG_ITEM_TRADE_BUY.
// Static 4-byte body -- confirmed the client never reads this field, so
// it's always sent as 1 rather than carrying a real reason code.
struct TradeBuyFail
{
    std::uint32_t failure = 1;

    void Serialize(PayloadWriter& writer) const;
};
