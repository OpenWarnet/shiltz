#pragma once

#include "CreatureAiState.h"

#include <cstdint>

class IdleState final : public CreatureAiState
{
public:
    static const CreatureAiState& Enter(CreatureAiData& data, bool delayed);

    [[nodiscard]] CreatureAiStateKind Kind() const noexcept override;
    [[nodiscard]] CreatureAiSenseKind Sense() const noexcept override;
    [[nodiscard]] CreatureAiDecision Decide(CreatureAiData& data, const Placement& self,
                                            const MonsterRecord& monster,
                                            const CreatureAiPerception& perception) const override;
};
