#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_STORE_MONEY_OUT (wire code 411061, c2s) -- withdraw money from the
// bank into the character's wallet.
struct StoreMoneyOut : PlayerMessage
{
    std::int64_t amount = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
