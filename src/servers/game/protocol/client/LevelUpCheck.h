#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_LEVEL_UP_CHECK (wire code 412016, c2s) -- client asks the server to
// check whether the character has crossed the exp threshold to level up.
struct LevelUpCheck : PlayerMessage
{
    std::int32_t session_id = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
