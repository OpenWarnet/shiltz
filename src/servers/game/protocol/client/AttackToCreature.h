#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_ATTACK_TO_CRT (wire code 0x064586 / 411014, c2s) -- a player's
// ordinary attack against one monster. The position is reported by the
// client and is validated against the authoritative Character position.
struct AttackToCreature : PlayerMessage
{
    std::uint32_t target_instance_id = 0;
    std::uint32_t direction = 0;
    std::uint32_t player_x = 0;
    std::uint32_t player_y = 0;
    std::uint32_t constant_one = 0;
    std::int32_t unknown = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
