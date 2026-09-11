#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_EMOTION (wire code 411074, c2s) -- client plays an emote animation.
struct Emotion : PlayerMessage
{
    std::int32_t emotion_id = 0;
    std::int32_t unknown = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
