#pragma once

#include <cstdint>

// GC_QUEST_FAIL's result_code -- see protocol/server/QuestFail.h. This
// single generic value is unverified, following the convention every other
// *_FAIL packet in this codebase uses (see e.g. enums/ItemConfirmFailReason.h)
// rather than a confirmed value -- there's currently no way to tell an
// unknown action_id apart from an unmet condition on the wire, so
// handlers/Quest.cpp sends this for both.
enum class QuestFailReason : std::int32_t
{
    ConditionsNotMet = 0,
};
