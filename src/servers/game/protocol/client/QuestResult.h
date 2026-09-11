#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_QUEST_RESULT (wire code 411026, c2s) -- client reports the outcome of
// a quest action against an NPC/creature (e.g. turning a quest in).
struct QuestResult : PlayerMessage
{
    std::uint32_t action_id = 0;
    std::uint32_t creature_instance_id = 0;
    // Third wire field -- present but not yet understood.
    std::uint32_t unknown = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
