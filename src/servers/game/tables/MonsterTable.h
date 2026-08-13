#pragma once

#include "parser/MonsterScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the monster.scr template table every Creature::monster_id joins
// against.
class MonsterTable
{
public:
    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    // Looks up a monster.scr row by MonsterRecord::id -- the same
    // id-space as Creature::monster_id. Returns nullptr if this table
    // hasn't loaded that id (e.g. a spawn file referencing an id that
    // isn't actually in monster.scr).
    const MonsterRecord* Find(std::int64_t monsterId) const;

private:
    std::unordered_map<std::int64_t, MonsterRecord> m_records;
};
