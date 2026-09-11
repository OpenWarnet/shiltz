#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_EMOTION_SUCC (wire code 521464, s2c) -- echoes CG_EMOTION back to the
// requesting connection. `unknown` is always sent 0 -- meaning not yet
// identified (see protocol/client/Emotion.h's field of the same name).
struct EmotionSucc : ServerProtocol
{
    std::uint32_t char_instance_id = 0;
    std::int32_t emotion_id = 0;
    std::int32_t unknown = 0;

    void Serialize(PayloadWriter& writer) const override;
};
