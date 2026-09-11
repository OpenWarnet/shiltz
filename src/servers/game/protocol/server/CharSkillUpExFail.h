#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CHAR_SKILL_UP_EX_FAIL (wire code 531102, s2c) -- rejects
// CG_CHAR_SKILL_UP_EX. `reason` is one of:
//   -1  DB error (reserved -- not currently surfaced, see handlers/CharSkillUp.cpp)
//   -2  total skill count error (empty request, or a non-positive num_level_up)
//   -3  target skill id doesn't exist
//   -4  prerequisite skill not learned
//   -5  skill already at max level
//   -6  character level too low for this skill
//   -7  not enough skill points
//   -8  skill's job doesn't match character's job
struct CharSkillUpExFail : ServerProtocol
{
    std::int32_t reason = 0;

    void Serialize(PayloadWriter& writer) const override;
};
