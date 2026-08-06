#pragma once

#include <cstdint>

class PayloadWriter;

// GC_TRADE_BUY_FAIL (wire code 531053, s2c) -- rejects CG_ITEM_TRADE_BUY.
// Static 4-byte body: a single reason code, no padding.
struct TradeBuyFail
{
    std::uint32_t reason = 1;

    void Serialize(PayloadWriter& writer) const;
};
