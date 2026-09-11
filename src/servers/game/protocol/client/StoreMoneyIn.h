#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_STORE_MONEY_IN (wire code 411060, c2s) -- deposit money from the
// character's wallet into the bank.
struct StoreMoneyIn : PlayerMessage
{
    std::int64_t amount = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
