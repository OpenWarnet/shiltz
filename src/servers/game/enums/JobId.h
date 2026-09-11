#pragma once

#include <cstdint>

// Real Seal Online job ids -- the same space Character::job_id and
// status.scr's class_id column use. 2nd/3rd-tier promotions of a base
// job collapse to that base job for growth-stat purposes (see
// ResolveStatusClassIndex in stats/RawStatCalculator.cpp); the *Tier2/*Tier3 values
// below are those promotion ids, not separate growth-stat identities.
enum class JobId : std::uint32_t
{
    Beginner = 0,
    Warrior = 1,
    Knight = 2,
    Clown = 3,
    Mage = 4,
    Priest = 5,
    Craftsman = 6,
    GameMaster = 7,
    Vagabond = 8,
    Hunter = 9,

    WarriorTier2 = 11,
    KnightTier2 = 12,
    ClownTier2 = 13,
    MageTier2 = 14,
    PriestTier2 = 15,
    CraftsmanTier2 = 16,
    HunterTier2 = 19,

    WarriorTier3 = 21,
    KnightTier3 = 22,
    ClownTier3 = 23,
    MageTier3 = 24,
    PriestTier3 = 25,
    CraftsmanTier3 = 26,
    HunterTier3 = 29,

    Cook = 31,
    CookTier2 = 131,
    CookTier3 = 231,
};
