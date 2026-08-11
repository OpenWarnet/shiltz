#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One row of an itemNN.scr file -- Seal Online's per-item template table
// (name, stats, pricing, description, ...). Unlike monster.scr, these rows
// mix text (item name) and floating-point columns alongside integers, so
// this doesn't keep a generic ParseInt64'd `fields` vector the way
// MonsterRecord does -- only the columns actually needed are named here.
// Field layout reverse-engineered from the client's item loader: column 0
// is id, column 2 is item_type, column 35 is buy_price, column 36 is
// sell_price. Add more named fields as more columns are needed.
//
// The ten `*_scale` fields (columns 10/16/21/24/26/28/30/32/38/40) are the
// per-item multipliers the NPC magic-option appraiser applies to a rolled
// tier (see handlers/ItemConfirmNpc.h) -- confirmed against this project's
// own item.scr/item*.scr data cross-referenced with the `unsealed` reader
// tool's independently-derived column mapping (all sample values matched
// exactly).
struct ItemRecord
{
    std::int64_t id = 0;
    std::int64_t item_type = 0;
    std::int64_t buy_price = 0;
    std::int64_t sell_price = 0;

    std::int64_t damage_scale = 0;
    std::int64_t magic_power_scale = 0;
    std::int64_t defense_scale = 0;
    std::int64_t attack_speed_scale = 0;
    std::int64_t accuracy_scale = 0;
    std::int64_t critical_rate_scale = 0;
    std::int64_t evasion_rate_scale = 0;
    std::int64_t movement_speed_scale = 0;
    std::int64_t hp_percent_scale = 0;
    std::int64_t ap_percent_scale = 0;
};

class ItemScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<ItemRecord> Load(const std::filesystem::path& path);
};
