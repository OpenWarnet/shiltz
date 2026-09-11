#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_STORE_MONEY_FAIL (wire code 531124, s2c) -- rejects either
// CG_STORE_MONEY_IN or CG_STORE_MONEY_OUT. Static 4-byte body, same
// "always 1, client doesn't read it" convention as TradeBuyFail/
// TradeSellFail.
struct StoreMoneyFail : ServerProtocol
{
    std::uint32_t failure = 1;

    void Serialize(PayloadWriter& writer) const override;
};
