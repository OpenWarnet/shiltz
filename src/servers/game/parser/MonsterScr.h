#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One row of monster.scr -- Seal Online's per-template table for every
// monster/NPC/entity in the world (stats, model, loot, dialogue hooks).
// `category` == 3 marks a non-attackable interactive entity (NPC, gacha,
// warp, statue, ...); attackable monsters use other values (0/1/2/6/...).
// `element` is a 1-based index (1 Fire, 2 Water, 3 Tree, 4 Steel, 5 Earth,
// 6 Sun, 7 Darkness, 8 Magical, 9 Physical; 0 none).
//
// Every column is a 64-bit value, one pipe-delimited decimal token per
// column, so column N sits at token index N directly. Only columns this
// project reads are named; the rest stay in `fields` (indexed by column,
// same as token index here) so columns with no meaning yet -- or added by
// a newer file version -- aren't dropped.
struct MonsterRecord
{
    std::int64_t id = 0;
    std::int64_t level = 0;
    std::int64_t hp = 0;
    std::int64_t movement_speed = 0;
    std::int64_t attack_range = 0;
    std::int64_t element = 0;
    std::int64_t critical_hit = 0;
    std::int64_t critical_hit_defense = 0;
    std::int64_t hit_rate = 0;
    std::int64_t evasion_rate = 0;
    std::int64_t attack = 0;
    std::int64_t defense = 0;
    std::int64_t exp = 0;
    std::int64_t loot_id = 0;
    std::int64_t category = 0;
    std::int64_t model_id = 0;
    std::int64_t talk_id = 0;
    std::int64_t seller_id = 0;
    std::int64_t respawn_time = 0;
    std::int64_t aggro_range = 0;

    std::vector<std::int64_t> fields;
};

class MonsterScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<MonsterRecord> Load(const std::filesystem::path& path);
};
