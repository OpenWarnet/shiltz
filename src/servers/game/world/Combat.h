#pragma once

#include <cstdint>

// Presentation supplied by CG_ATTACK_TO_CRT. The reported position is never
// authoritative; CombatSystem validates it against the Character before the
// attack reaches the domain operation.
struct BasicAttack
{
    std::uint32_t direction = 0;
    std::uint32_t reported_x = 0;
    std::uint32_t reported_y = 0;
};

enum class AttackOutcome : std::uint8_t
{
    Hit,
    Killed,
};

struct AttackResult
{
    AttackOutcome outcome = AttackOutcome::Hit;
    std::uint32_t damage = 0;
    std::uint64_t target_hp = 0;
    std::int64_t exp_gain = 0;
};

// Result of applying one already-calculated damage amount to a monster.
// The monster owns the Alive -> Dead transition and supplies its configured
// reward; the attacking Player owns applying that reward to itself.
struct DamageResult
{
    std::uint32_t damage = 0;
    std::uint64_t target_hp = 0;
    std::int64_t exp_reward = 0;
    bool killed = false;
};
