#pragma once

#include <cstdint>

// GC_CHAR_SKILL_UP_EX_FAIL's reason codes -- see
// protocol/server/CharSkillUpExFail.h for the full table.
enum class SkillUpFailReason : std::int32_t
{
    DbError = -1,
    TotalSkillCountError = -2,
    SkillIdNotFound = -3,
    PrereqNotLearned = -4,
    AlreadyMaxLevel = -5,
    LevelTooLow = -6,
    NotEnoughSp = -7,
    JobMismatch = -8,
};
