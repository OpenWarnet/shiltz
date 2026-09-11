#pragma once

#include "protocol/ClientProtocol.h"

#include <string>

class PayloadReader;

// CG_STORE_PW_MODIFY (wire code 411055, c2s) -- change the bank password.
// Both fields are plaintext, same 16-byte fixed convention as StoreOpen.
struct StorePwModify : PlayerMessage
{
    std::string old_password;
    std::string new_password;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
