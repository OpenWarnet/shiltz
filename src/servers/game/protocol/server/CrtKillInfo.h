#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CRT_KILL_INFO (wire code 0x07CE34 / 511540, s2c) -- EXP and
// kill-counter notification sent to every player viewing a monster kill.
// The 20-byte layout is confirmed by ten captures; counter semantics come
// from the client labels CurNumber/MaxNumber/MultipleNumber.
struct CrtKillInfo : ServerMessage<GameOpcode::GC_CRT_KILL_INFO>
{
    std::int64_t exp_gain = 0;
    std::uint32_t current_kill_count = 0;
    std::uint32_t max_kill_count = 0;
    std::uint32_t multiple_kill_count = 0;

    void Serialize(PayloadWriter& writer) const override;
};
