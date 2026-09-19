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
// `ai_id`/`secondary_ai_id` join AiMonRecord::id (ai_mon.scr) -- the
// reverse-engineered reference calls these iAI_index / iAI_Index_On_Death.
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
    std::int64_t wander_step_count = 0;
    std::int64_t attack_range = 0;
    std::int64_t element = 0;
    std::int64_t critical_hit_chance = 0;
    std::int64_t critical_hit_defense = 0;
    std::int64_t hit_rate = 0;
    std::int64_t evasion_rate = 0;
    std::int64_t attack = 0;
    std::int64_t defense = 0;
    std::int64_t exp_reward = 0;
    std::int64_t loot_id = 0;
    std::int64_t ai_id = 0;
    std::int64_t category = 0;
    std::int64_t model_id = 0;
    std::int64_t talk_id = 0;
    std::int64_t seller_id = 0;
    std::int64_t pack_flag = 0; // meaning unconfirmed
    std::int64_t secondary_ai_id = 0;
    std::int64_t unique_spawn_flag = 0;
    std::int64_t buff_gold_reward = 0;
    std::int64_t respawn_time = 0;
    std::int64_t spawn_scatter_range = 0;
    std::int64_t aggro_range = 0;
    std::int64_t link_flag = 0;

    std::vector<std::int64_t> fields;
};

class MonsterScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<MonsterRecord> Load(const std::filesystem::path& path);
};
