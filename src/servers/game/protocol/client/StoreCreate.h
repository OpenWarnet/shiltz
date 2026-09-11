#pragma once

#include "protocol/ClientProtocol.h"

#include <string>

class PayloadReader;

// CG_STORE_CREATE (wire code 411053, c2s) -- request to create the "bank"
// storage feature for this account, presenting the plaintext password to
// provision bank_accounts with (see handlers/Store.cpp).
struct StoreCreate : PlayerMessage
{
    std::string password;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
