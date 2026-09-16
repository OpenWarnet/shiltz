#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_ATTACK_TO_CRT_SUCC (wire code 0x07F339 / 521017, s2c) -- private
// ordinary-hit result sent to the attacking player. The five-u32, 20-byte
// layout is confirmed by paired live captures.
struct AttackToCreatureSuccess : ServerMessage<GameOpcode::GC_ATTACK_TO_CRT_SUCC>
{
    std::uint32_t target_instance_id = 0;
    std::uint32_t damage = 0;
    std::uint32_t target_hp = 0;
    std::uint32_t unknown_1 = 0;
    std::uint32_t attacker_hit_counter = 0;

    void Serialize(PayloadWriter& writer) const override;
};
