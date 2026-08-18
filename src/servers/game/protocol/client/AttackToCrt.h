#pragma once

#include <cstdint>

class PayloadReader;

// CG_ATTACK_TO_CRT (wire code 411014, c2s) -- client attacks a creature by
// instance id. 24 bytes, 6x u32. Field order confirmed against 62 real
// samples (min/max range per position): target_id sits in creature-id
// range and stays constant across an attack sequence; direction matches
// the game's 1..8 compass encoding; pos_x/pos_y track a coordinate that
// moves with the player mid-fight (attacker's or target's tile -- the two
// coincide at melee range, so which one this is stays undetermined).
// unknown1 is constant 1 in every sample; unknown2's role is unresolved
// (its range includes negative values, so it isn't a plain count/id).
struct AttackToCrt
{
    std::uint32_t target_id = 0;
    std::uint32_t direction = 0;
    std::uint32_t pos_x = 0;
    std::uint32_t pos_y = 0;
    std::uint32_t unknown1 = 0;
    std::int32_t unknown2 = 0;

    bool Deserialize(PayloadReader& reader);
};
