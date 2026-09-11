#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_ITEM_TRADE_SELL (wire code 411021, c2s) -- request to sell an
// inventory item back to an NPC shop.
struct ItemTradeSell : PlayerMessage
{
    // Source inventory slot, wire-relative -- same convention as
    // ItemPickup::slot_id (bag slots start at InventoryItemList::kBagStartSlot).
    std::uint32_t slot_id = 0;
    std::uint32_t count = 0;
    // The shop NPC's Creature::instance_id -- same field as
    // ItemTradeBuy::creature_instance_id, named as given on the wire.
    std::uint32_t instance_id = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
