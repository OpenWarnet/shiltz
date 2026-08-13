#pragma once

#include "parser/SkillScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the skillNN.scr per-level skill attribute table.
class SkillTable
{
public:
    // skillNN.scr is split one file per skill level (skill01.scr = every
    // skill's attributes at level 1, ...), same "load every .scr file in
    // the directory" approach as ItemTable -- the level comes from the
    // filename, not a column in the row. Throws std::runtime_error if a
    // file can't be opened.
    void Load(const std::filesystem::path& dir);

    // Looks up a skillNN.scr row by (SkillRecord::id, level) -- level comes
    // from which skillNN.scr file the row was loaded from (see Load),
    // the same id-space as PlayerSkill::level. Returns nullptr if this
    // table hasn't loaded that skill/level combination.
    const SkillRecord* Find(std::int64_t skillId, std::int64_t level) const;

private:
    // Keyed by (skillId << 32) | level -- see MakeKey in SkillTable.cpp.
    std::unordered_map<std::int64_t, SkillRecord> m_records;
};
