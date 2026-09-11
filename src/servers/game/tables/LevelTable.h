#pragma once

#include "parser/LevelScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the level.scr exp-requirement table.
class LevelTable
{
public:
    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    // Looks up a level.scr row by LevelRecord::level -- the same id-space
    // as Character::level. Returns nullptr if this table hasn't loaded that
    // level (e.g. it's past the max level in the table).
    const LevelRecord* Find(std::int64_t level) const;

private:
    std::unordered_map<std::int64_t, LevelRecord> m_records;
};
