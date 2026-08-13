#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One row of set_opt.scr -- Seal Online's equipped-set bonus table. Each
// row grants a bonus for wearing exactly `piece_count` items that share the
// same item.scr set id (ItemRecord::set_id, column 33) at once -- not every
// (set_id, piece_count) pair has a row (e.g. set 255, the "Imitation of
// Tiphareth's" set used by character id 3's gear, has only a single row,
// for wearing all 4 pieces; no partial-set bonus exists for it).
//
// The ten stat fields are in the same order as ItemRecord's per-item
// `*_bonus`/`*_scale` fields (damage, magic, defense, attack_speed,
// accuracy, critical_rate, evasion, movement_speed, hp_percent,
// ap_percent), and add on top of the equipped items' own
// ItemRecord::*_bonus columns rather than replacing them (see
// stats/EquipmentStatCalculator.cpp's set-bonus pass). Five more numeric
// columns follow in the raw file with no confirmed meaning yet -- not
// parsed.
struct SetOptionRecord
{
    std::int64_t set_id = 0;
    std::int64_t piece_count = 0;

    std::int64_t damage = 0;
    std::int64_t magic = 0;
    std::int64_t defense = 0;
    std::int64_t attack_speed = 0;
    std::int64_t accuracy = 0;
    std::int64_t critical_rate = 0;
    std::int64_t evasion = 0;
    std::int64_t movement_speed = 0;
    std::int64_t hp_percent = 0;
    std::int64_t ap_percent = 0;
};

class SetOptScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<SetOptionRecord> Load(const std::filesystem::path& path);
};
