#include "Player.h"

#include "world/Creature.h"
#include "world/common/EventBus.h"
#include "world/events/CombatEvents.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <iostream>

namespace
{
constexpr std::uint32_t kBasicAttackRange = 1;
} // namespace

std::optional<AttackResult> Player::Attack(Creature& target, const BasicAttack& attack)
{
    if (!m_events || target.m_events != m_events)
    {
        return std::nullopt;
    }

    const Placement reported{
        .x = attack.reported_x,
        .y = attack.reported_y,
        .direction = static_cast<Direction>(attack.direction),
    };

    if (!reported.IsAt(character.placement) ||
        !reported.IsWithinRange(target.placement, kBasicAttackRange))
    {
        return std::nullopt;
    }

    // This first vertical slice deliberately resolves ordinary hits only.
    // Accuracy, criticals, elements, and weapon-specific multipliers can
    // extend this calculation without changing death ownership or events.
    const std::int64_t attackPower = std::max<std::int64_t>(1, character.stats.derived.damage);
    const std::int64_t targetDefense =
        std::max<std::int64_t>(0, target.monster_template.get().defense);
    const std::int64_t requestedDamage = std::max<std::int64_t>(1, attackPower - targetDefense);

    const std::optional<DamageResult> damage =
        target.TakeDamage(static_cast<std::uint32_t>(std::min<std::int64_t>(
                              requestedDamage, std::numeric_limits<std::uint32_t>::max())),
                          character.instance_id);
    if (!damage)
    {
        return std::nullopt;
    }

    character.placement.direction = reported.direction;

    const std::int64_t expGain = damage->killed ? GainExperience(damage->exp_reward) : 0;

    const AttackResult result{
        .outcome = damage->killed ? AttackOutcome::Killed : AttackOutcome::Hit,
        .damage = damage->damage,
        .target_hp = damage->target_hp,
        .exp_gain = expGain,
    };

    m_events->Publish(PlayerAttackResolvedEvent{
        .attacker_id = character.instance_id,
        .target_id = target.instance_id,
        .direction = attack.direction,
        .origin_x = character.placement.x,
        .origin_y = character.placement.y,
        .target_x = target.placement.x,
        .target_y = target.placement.y,
        .damage = result.damage,
        .target_hp = result.target_hp,
        .exp_gain = result.exp_gain,
        .outcome = result.outcome,
    });

    return result;
}

std::int64_t Player::GainExperience(std::int64_t reward) noexcept
{
    const std::int64_t current = std::max<std::int64_t>(0, character.exp);
    reward = std::max<std::int64_t>(0, reward);

    const std::int64_t available = std::numeric_limits<std::int64_t>::max() - current;
    const std::int64_t granted = std::min(reward, available);
    character.exp = current + granted;
    return granted;
}

void Player::Bind(EventBus& events) noexcept
{
    m_events = &events;
}

void Player::Unbind() noexcept
{
    m_events = nullptr;
}
