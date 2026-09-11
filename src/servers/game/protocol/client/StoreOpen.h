#pragma once

#include "protocol/ClientProtocol.h"

#include <string>

class PayloadReader;

// CG_STORE_OPEN (wire code 411054, c2s) -- request to open the "bank"
// storage feature, presenting its plaintext password (see
// handlers/Store.cpp, which compares it against bank_accounts.password).
struct StoreOpen : PlayerMessage
{
    std::string password;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
