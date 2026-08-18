#pragma once

#include <cstdint>

class PayloadWriter;

// GC_ATTACK_CRT2TARGET_MISS (wire code 531024, s2c) -- a creature's melee
// swing at its current attack target, stubbed to always report a miss (no
// damage model exists yet -- see Map::AdvanceAttackState). Field shape
// mirrors GC_ATTACK_CRT2TARGET_SUCC's own leading fields (see OpenShiltz's
// gc_attack_crt2target_succ.py) minus the hit-result slots a miss has
// nothing to fill: target_id is who it's attacking; direction/pos_x/pos_y
// are the creature's own facing/position at the moment of the swing.
// unknown's role isn't confirmed from a live capture.
struct AttackCrt2TargetMiss
{
    std::uint32_t target_id = 0;
    std::uint32_t direction = 0;
    std::uint32_t pos_x = 0;
    std::uint32_t pos_y = 0;
    std::int32_t unknown = 0;

    void Serialize(PayloadWriter& writer) const;
};
