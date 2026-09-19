#pragma once

#include "CreatureAiTypes.h"
#include "states/CreatureAiState.h"

#include <chrono>
#include <optional>

struct MonsterRecord;

// Per-monster AI coordinator. States are shared stateless behavior; this class
// owns the creature's AI data and tracks the current state.
class CreatureAi
{
public:
    CreatureAi();

    void Reset();

    // Returns a request only when the current state is due to make progress.
    std::optional<CreatureAiSenseRequest> Advance(std::chrono::milliseconds delta);

    std::optional<CreatureAiIntent> Decide(const Placement& self, const MonsterRecord& monster,
                                           const CreatureAiPerception& perception);

    // Map calls this after attempting a wander movement.
    void CompleteWanderStep();

    [[nodiscard]] CreatureAiStateKind State() const noexcept;

private:
    CreatureAiData m_data;
    const CreatureAiState* m_state;
};
