#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_ITEM_DELETE (wire code 0x06473E, c2s) -- delete an item from
// equipment/inventory outright (no ground item is created, unlike
// CG_ITEM_DROP).
//
// Wire layout: slot_id (same wire-relative slot convention as
// ItemPickup/ItemMove/ItemDrop -- equipment is 0-12, bag slots start at
// InventoryItemList::kBagStartSlot), then a u32 that's always 1 in every
// capture seen (not branched on), then a trailing u32 that's always 0
// (unused padding, not a field) -- confirmed against 4 real captures
// decoded via OpenShiltz's tooling.
struct ItemDelete : PlayerMessage
{
    std::uint32_t slot_id = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
