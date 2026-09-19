#pragma once

#include "world/ai/CreatureAiTypes.h"

#include <optional>

struct MonsterRecord;
class CreatureAiState;

// A state proposes a world action and enters its successor, which may be itself.
struct CreatureAiDecision
{
    std::optional<CreatureAiIntent> intent;
    const CreatureAiState& next;
};

// Stateless behavior shared by every creature; per-creature data lives in CreatureAiData.
class CreatureAiState
{
public:
    [[nodiscard]] virtual CreatureAiStateKind Kind() const noexcept = 0;

    // What Map should sense once the state's timer elapses.
    [[nodiscard]] virtual CreatureAiSenseKind Sense() const noexcept = 0;

    // Returns a state reached through Enter, which resets data for that state.
    [[nodiscard]] virtual CreatureAiDecision
    Decide(CreatureAiData& data, const Placement& self, const MonsterRecord& monster,
           const CreatureAiPerception& perception) const = 0;

protected:
    ~CreatureAiState() = default;
};
