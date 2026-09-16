#pragma once

#include "world/Combat.h"

#include <cstdint>

// A monster made its one-way Alive -> Dead transition. The monster publishes
// this at the transition site so no damage caller can forget the death event.
struct MonsterKilledEvent
{
    std::uint32_t monster_id = 0;
    std::uint32_t killer_id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::int64_t exp_reward = 0;
};

// A validated player basic attack has fully committed its monster damage and,
// on a lethal hit, its EXP gain. CombatSystem turns this semantic result into
// the private TO_CRT result and public PC2CRT presentation packet.
struct PlayerAttackResolvedEvent
{
    std::uint32_t attacker_id = 0;
    std::uint32_t target_id = 0;
    std::uint32_t direction = 0;
    std::uint32_t origin_x = 0;
    std::uint32_t origin_y = 0;
    std::uint32_t target_x = 0;
    std::uint32_t target_y = 0;
    std::uint32_t damage = 0;
    std::uint64_t target_hp = 0;
    std::int64_t exp_gain = 0;
    AttackOutcome outcome = AttackOutcome::Hit;
};
