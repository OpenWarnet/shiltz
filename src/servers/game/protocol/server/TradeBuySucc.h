#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_TRADE_BUY_SUCC (wire code 521052, s2c) -- acknowledges a successful
// CG_ITEM_TRADE_BUY.
struct TradeBuySucc : ServerMessage<GameOpcode::GC_TRADE_BUY_SUCC>
{
    std::uint32_t slot_id = 0;
    std::uint32_t item_id = 0;
    std::uint32_t new_count = 0;
    std::uint32_t option = 0;
    std::uint32_t option2 = 0;
    std::int64_t money = 0;

    void Serialize(PayloadWriter& writer) const override;
};
