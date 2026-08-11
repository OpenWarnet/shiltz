#pragma once

#include <cstdint>

// item.scr/item*.scr's item_type column (this project's confirmed column
// index: 2). Only the values the NPC magic-option appraiser cares about
// (handlers/ItemConfirmNpc.h) are named here. Potion/Misc come from the
// `unsealed` reader tool's independently-derived binary-format mapping
// (the column index was cross-checked against this project's own
// item*.scr data and matches, but these two names weren't independently
// re-verified per value against this text format); PetEgg is confirmed
// directly (item 99, "Piya's Egg", has item_type 22 in item.scr).
// Type2/Type23/Type24/Type27 are genuinely unconfirmed -- named after
// their raw value rather than a guessed meaning.
enum class ItemType : std::int64_t
{
    Material = 0,
    Potion = 1,
    Type2 = 2,
    Misc = 3,

    PetEgg = 22, // 0x16
    Type23 = 23, // 0x17
    Type24 = 24, // 0x18
    Type27 = 27, // 0x1B
};
