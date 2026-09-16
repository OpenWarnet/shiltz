#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_ATTACK_TO_CRT_KILL (wire code 0x07F33B / 521019, s2c) -- private
// lethal-hit result. The 36-byte layout is confirmed by eight captures;
// exp_gain is the reward delta and matches GC_CRT_KILL_INFO in paired kills.
struct AttackToCreatureKill : ServerMessage<GameOpcode::GC_ATTACK_TO_CRT_KILL>
{
    std::uint32_t target_instance_id = 0;
    std::uint32_t target_damage = 0;
    std::uint64_t target_hp = 0;
    std::int64_t exp_gain = 0;
    std::uint32_t unknown_1 = 0;
    std::uint32_t unknown_2 = 0;
    std::uint32_t display_damage = 0;

    void Serialize(PayloadWriter& writer) const override;
};
