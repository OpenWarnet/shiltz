#include "AttackingState.h"

#include "ChaseState.h"
#include "IdleState.h"
#include "StateSupport.h"
#include "parser/MonsterScr.h"

namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kAttackDelay = 1000ms;
} // namespace

const CreatureAiState& AttackingState::Enter(CreatureAiData& data, std::uint32_t targetId)
{
    static const AttackingState instance;

    data = CreatureAiData{};
    data.target_id = targetId;
    data.timer = kAttackDelay;

    return instance;
}

CreatureAiSenseKind AttackingState::Sense() const noexcept
{
    return CreatureAiSenseKind::CurrentTarget;
}

CreatureAiDecision AttackingState::Decide(CreatureAiData& data, const Placement& self,
                                          const MonsterRecord& monster,
                                          const CreatureAiPerception& perception) const
{
    if (!perception.current_target)
        return {std::nullopt, IdleState::Enter(data, false)};

    // Entering a state resets data, so read the target first.
    const std::uint32_t targetId = data.target_id;
    const CreatureAiObservation& target = *perception.current_target;
    const std::uint32_t distance = self.DistanceTo(target.placement);
    const std::uint32_t aggroRange = creature_ai::NormalizeMonsterValue(monster.aggro_range);
    if (aggroRange == 0 || distance > aggroRange)
        return {std::nullopt, IdleState::Enter(data, false)};

    if (distance > creature_ai::NormalizeMonsterValue(monster.attack_range))
    {
        CreatureChaseIntent chase{target.player_id, target.placement};
        return {chase, ChaseState::Enter(data, targetId, true)};
    }

    return {CreatureAttackIntent{targetId}, AttackingState::Enter(data, targetId)};
}

CreatureAiStateKind AttackingState::Kind() const noexcept
{
    return CreatureAiStateKind::Attacking;
}

