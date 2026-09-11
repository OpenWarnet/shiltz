#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>
#include <vector>

class PayloadReader;

// CG_ITEM_CONFIRM_NPC_REQUEST (wire code 0x0649DB, c2s) -- client asks the
// NPC magic-option appraiser to reveal/roll the hidden magic options on
// one or more inventory/equipment slots.
//
// Wire layout: int32 count, then npc_flag (normally 1; a 0 case exists
// but its meaning isn't confirmed -- parsed and kept, not branched on),
// then `count` u32 slot ids (an inventory/equipment slot index, not an
// item id), then a trailing u32 that's always 0 (unused padding, not a
// field). count must be in [1,8] (see handlers/ItemConfirmNpc.cpp).
struct ItemConfirmNpcRequest : ClientProtocol
{
    std::uint32_t npc_flag = 1;
    std::vector<std::uint32_t> slot_ids;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
