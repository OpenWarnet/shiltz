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
//
// The ten `*_scale` fields are per-item multipliers the NPC magic-option
// appraiser applies to a rolled tier (see handlers/ItemConfirmNpc.h). The
// ten `*_bonus` fields are a separate, always-on flat stat grant from
// simply wearing the item, independent of the appraiser system above
// (world/Stats.cpp sums both). hp_bonus/ap_bonus in particular are NOT the
// same thing as hp_percent_scale/ap_percent_scale: the bonus fields are a
// flat point grant, the scale fields are a percent-of-roll input to the
// unrelated appraiser system.
//
// `damage_dealt_increase_percent_bonus` and
// `damage_taken_decrease_percent_bonus` are percentages, not flat point
// grants like the other `*_bonus` fields -- and despite both having
// "damage" in the name, they modify opposite directions of combat: dealt
// is a bonus to damage *this character* deals to a target, taken is a
// reduction to damage *this character* receives from an attacker (see
// PlayerDerivedStats::damage_dealt_increase_percent/
// damage_taken_decrease_percent in world/Player.h).
//
// `set_id` groups items into an equipped-set bonus (0 = not part of a
// set) -- world/data/set_opt.scr has one row per (set_id, piece_count)
// actually worn together, see parser/SetOptScr.h and world/Stats.cpp's
// set-bonus pass.
struct ItemRecord
{
    std::int64_t id = 0;
    std::int64_t item_type = 0;
    std::int64_t buy_price = 0;
    std::int64_t sell_price = 0;

    std::int64_t damage_bonus = 0;
    std::int64_t damage_dealt_increase_percent_bonus = 0;
    std::int64_t defense_bonus = 0;
    std::int64_t magic_power_bonus = 0;
    std::int64_t damage_taken_decrease_percent_bonus = 0;
    std::int64_t attack_speed_bonus = 0;
    std::int64_t accuracy_bonus = 0;
    std::int64_t critical_rate_bonus = 0;
    std::int64_t evasion_rate_bonus = 0;
    std::int64_t movement_speed_bonus = 0;
    std::int64_t hp_bonus = 0;
    std::int64_t ap_bonus = 0;

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

    std::int64_t set_id = 0;

    // Refine ("+N") growth inputs -- a separate multiplier system from the
    // `*_scale` fields above (those feed the appraiser roll only). Growth
    // at a given refine level = a per-(refine_group, level) curve shared
    // by every item with that refine_group, multiplied by the item's own
    // refine_damage_scale/refine_magic_scale/refine_defense_scale for that
    // stat (see world/Stats.cpp's kRefineCurve* tables); a scale of 0
    // means that stat doesn't grow on this item at all.
    std::int64_t refine_damage_scale = 0;
    std::int64_t refine_magic_scale = 0;
    std::int64_t refine_defense_scale = 0;
    std::int64_t refine_group = 0;
};

class ItemScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<ItemRecord> Load(const std::filesystem::path& path);
};
