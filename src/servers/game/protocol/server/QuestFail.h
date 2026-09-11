#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_QUEST_FAIL (wire code 531065, s2c) -- rejects CG_QUEST_RESULT when the
// reported action_id doesn't exist in quest.scr, or exists but its
// CONDITIONS aren't met. See enums/QuestFailReason.h -- the payload shape
// here (single result_code) is unverified, following the convention every
// other *_FAIL packet in this codebase uses.
struct QuestFail : ServerMessage<GameOpcode::GC_QUEST_FAIL>
{
    std::int32_t result_code = 0;

    void Serialize(PayloadWriter& writer) const override;
};
