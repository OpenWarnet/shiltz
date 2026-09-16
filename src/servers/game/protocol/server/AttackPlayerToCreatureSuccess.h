#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_ATTACK_PC2CRT_SUCC (wire code 0x07F33C / 521020, s2c) -- public
// player-hit presentation sent to the attacker and nearby observers.
struct AttackPlayerToCreatureSuccess : ServerMessage<GameOpcode::GC_ATTACK_PC2CRT_SUCC>
{
    std::uint32_t attacker_instance_id = 0;
    std::uint32_t direction = 0;
    std::uint32_t player_x = 0;
    std::uint32_t player_y = 0;
    std::uint32_t constant_one = 1;
    std::uint32_t target_instance_id = 0;
    std::uint32_t damage = 0;
    std::uint32_t target_hp = 0;
    std::uint32_t attacker_hit_counter = 0;

    void Serialize(PayloadWriter& writer) const override;
};
