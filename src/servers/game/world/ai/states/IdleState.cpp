#include "IdleState.h"

#include "ChaseState.h"
#include "WanderState.h"
#include "StateSupport.h"
#include "common/Random.h"
#include "parser/MonsterScr.h"

namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kBaseDelay = 2000ms;
constexpr std::chrono::milliseconds kMaximumJitter = 1000ms;
} // namespace

const CreatureAiState& IdleState::Enter(CreatureAiData& data, bool delayed)
{
    static const IdleState instance;

    data = CreatureAiData{};
    if (delayed)
        data.timer = creature_ai::WithJitter(kBaseDelay, kMaximumJitter);

    return instance;
}

CreatureAiSenseKind IdleState::Sense() const noexcept
{
    return CreatureAiSenseKind::AggroCandidate;
}

CreatureAiDecision IdleState::Decide(CreatureAiData& data, const Placement&,
                                     const MonsterRecord& monster,
                                     const CreatureAiPerception& perception) const
{
    if (perception.aggro_candidate)
    {
        const std::uint32_t targetId = perception.aggro_candidate->player_id;
        return {std::nullopt, ChaseState::Enter(data, targetId, false)};
    }

    const std::uint32_t steps = creature_ai::NormalizeMonsterValue(monster.wander_step_count);
    if (steps > 0 && (Random::Next() & 1) != 0)
        return {std::nullopt, WanderState::Enter(data, steps)};

    return {std::nullopt, IdleState::Enter(data, true)};
}

CreatureAiStateKind IdleState::Kind() const noexcept
{
    return CreatureAiStateKind::Idle;
}

