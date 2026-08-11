#pragma once

#include <cstdint>

// CG_CHAR_STATUS_UP's stat_id, matching the client's own wire order (see
// CharacterDataLoad's stats_str..stats_sen fields), 1-based.
enum class StatId : std::int32_t
{
    Strength = 1,
    Intelligence = 2,
    Dexterity = 3,
    Constitution = 4,
    Mentality = 5,
    Sense = 6,
};
