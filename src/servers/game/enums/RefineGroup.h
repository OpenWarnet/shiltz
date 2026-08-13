#pragma once

#include <cstdint>

// item.scr/item*.scr's refine_group column -- picks which refine ("+N")
// growth curve an item uses (see world/Item.cpp's kRefineCurveArmor/
// kRefineCurveWeapon). Only the confirmed values are named here; every
// other raw value seen in the data (0, 1, 3, 5, 7+) currently falls
// through to "no refine growth curve" in Item::CalculateRefineContribution.
enum class RefineGroup : std::int64_t
{
    None = 0, // majority default -- not refinable / no growth curve

    Weapon = 2, // refine_damage_scale/refine_magic_scale growth (kRefineCurveWeapon),
                // plus the weapon-only damage_dealt_increase_percent curve

    // Both confirmed to use the same growth curve (kRefineCurveArmor,
    // refine_damage_scale/refine_magic_scale/refine_defense_scale) --
    // named by raw value since there's no confirmed semantic difference
    // between them yet (e.g. distinct armor slot categories).
    Armor4 = 4,
    Armor6 = 6,
};
