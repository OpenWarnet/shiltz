#pragma once

#include <cstdint>

class PayloadWriter;

// GC_CHAR_SKILL_UP_EX_SUCC (wire code 521100, s2c) -- confirms
// CG_CHAR_SKILL_UP_EX, reporting the character's sp/ep totals after
// spending sp on the requested skill level-ups.
struct CharSkillUpExSucc
{
    std::int32_t remaining_sp = 0;
    std::int32_t remaining_ep = 0;

    void Serialize(PayloadWriter& writer) const;
};
