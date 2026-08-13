#pragma once

#include <cstdint>

// item.scr/item*.scr's item_type column. Only the values the NPC
// magic-option appraiser cares about (handlers/ItemConfirmNpc.h) are named
// here. Type2/Type23/Type24/Type27's meanings aren't confirmed, so they're
// named by raw value rather than a possibly-wrong descriptive name.
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
