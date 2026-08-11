#pragma once

#include <cstdint>
#include <vector>

class PayloadReader;

// One {num_level_up, skill_id} entry inside CG_CHAR_SKILL_UP_EX's request
// array -- the client can bundle several skills into a single request.
struct SkillLevelUpEntry
{
    std::int32_t num_level_up = 0;
    std::int32_t skill_id = 0;

    bool Deserialize(PayloadReader& reader);
};

// GC_CHAR_SKILL_UP_EX (wire code 411059, c2s) -- see GameOpcodes.h for why
// this client request is named "GC_". Asks to raise one or more skills by
// num_level_up levels each, spending unallocated sp. Wire layout: int32
// count, then `count` SkillLevelUpEntry pairs, then a trailing int32
// that's always 0 (unused terminator/padding).
struct CharSkillUpEx
{
    std::vector<SkillLevelUpEntry> skills;

    bool Deserialize(PayloadReader& reader);
};
