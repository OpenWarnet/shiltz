#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// One row of a skillNN.scr file -- Seal Online's per-level skill attribute
// table. `id` is the skill id (joins against PlayerSkill::id/
// SkillLevelUpEntry::skill_id); the level itself isn't a column in the row
// -- it's implied by which skillNN.scr file the row came from (skill01.scr
// = level 1's attributes for every skill, skill02.scr = level 2, ...), so
// callers must track it alongside SkillScr::Load's return value (see
// World::Start).
//
// Field layout/order matches unsealed's SKILL_BAND_SCHEMA (46 pipe-
// delimited columns, one skillNN.scr row per skill) -- named fields are
// carried over verbatim from that reverse-engineering; `field_N` names mark
// columns whose meaning isn't known yet. Row 0 of every file is an all-zero
// placeholder (id=0, name="skill").
struct SkillRecord
{
    std::int64_t id = 0;
    std::string name;
    std::int64_t job_type = 0;
    std::int64_t skill_type = 0;
    std::int64_t field_4 = 0;
    std::string key;

    std::int64_t prereq_skill_id = 0;
    std::int64_t prereq_skill_level = 0;
    std::int64_t max_skill_level = 0;
    std::int64_t skill_points = 0;
    std::int64_t min_level = 0;
    std::int64_t required_equip_type = 0;
    std::int64_t field_10 = 0;
    std::int64_t ap_cost = 0;
    std::int64_t support_subtype = 0;
    std::int64_t cast_num_target = 0;
    std::int64_t area_of_effect = 0;
    std::int64_t cast_range = 0;

    double casting_time = 0.0;
    double duration_seconds = 0.0;
    double cooldown_seconds = 0.0;

    std::int64_t number_of_hits = 0;
    std::int64_t damage_pct = 0;
    std::int64_t element = 0;
    std::int64_t field_23 = 0;

    std::int64_t field_24 = 0;
    std::int64_t linked_skill_1 = 0;
    std::int64_t buff_1_id = 0;
    std::int64_t buff_1_duration_seconds = 0;
    std::int64_t buff_1_chance = 0;
    std::int64_t linked_skill_2 = 0;
    std::int64_t buff_2_id = 0;
    std::int64_t buff_2_duration_seconds = 0;
    std::int64_t buff_2_chance = 0;
    std::int64_t buff_3_id = 0;
    std::int64_t buff_4_id = 0;
    std::int64_t field_35 = 0;
    std::int64_t field_36 = 0;

    std::int64_t icon_id = 0;
    std::int64_t projectile_speed = 0;
    std::int64_t field_39 = 0;

    std::int64_t field_40 = 0;

    double ultimate_move_pct = 0.0;
    double field_42 = 0.0;

    std::int64_t field_43 = 0;
    std::string description;
};

class SkillScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<SkillRecord> Load(const std::filesystem::path& path);
};
