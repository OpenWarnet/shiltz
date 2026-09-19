#include "WanderState.h"

#include "IdleState.h"
#include "StateSupport.h"
#include "common/Random.h"

#include <array>
#include <utility>

namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kStepBaseDelay = 5000ms;
constexpr std::chrono::milliseconds kStepMaximumJitter = 1000ms;

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> kOffsets{{
    {-1, -1},
    {0, -1},
    {1, -1},
    {-1, 0},
    {1, 0},
    {-1, 1},
    {0, 1},
    {1, 1},
}};
} // namespace

const CreatureAiState& WanderState::Enter(CreatureAiData& data, std::uint32_t steps)
{
    static const WanderState instance;

    data = CreatureAiData{};
    data.steps_remaining = steps;

    return instance;
}

CreatureAiSenseKind WanderState::Sense() const noexcept
{
    return CreatureAiSenseKind::None;
}

CreatureAiDecision WanderState::Decide(CreatureAiData& data, const Placement&,
                                       const MonsterRecord&, const CreatureAiPerception&) const
{
    if (data.steps_remaining == 0)
        return {std::nullopt, IdleState::Enter(data, false)};

    const auto [dx, dy] = kOffsets[Random::Below(kOffsets.size())];
    return {CreatureWanderIntent{dx, dy}, *this};
}

bool WanderState::CompleteStep(CreatureAiData& data)
{
    if (data.steps_remaining > 0)
        --data.steps_remaining;

    if (data.steps_remaining == 0)
    {
        data.timer = std::chrono::milliseconds::zero();
        return true;
    }

    data.timer = creature_ai::WithJitter(kStepBaseDelay, kStepMaximumJitter);
    return false;
}

CreatureAiStateKind WanderState::Kind() const noexcept
{
    return CreatureAiStateKind::Wander;
}

