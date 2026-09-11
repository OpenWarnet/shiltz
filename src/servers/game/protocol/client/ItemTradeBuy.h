#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_ITEM_TRADE_BUY (wire code 411020, c2s) -- request to buy an item from
// an NPC shop.
struct ItemTradeBuy : ClientProtocol
{
    // seller.scr row this shop's listing comes from (SellerRecord::shop_id).
    std::uint32_t shop_id = 0;
    // Index into SellerRecord::items (0-based) identifying which listed
    // item slot was bought -- NOT an inventory slot.
    std::uint32_t item_buy_index = 0;
    std::uint32_t amount = 0;
    // Target inventory slot, chosen client-side -- same convention as
    // ItemPickup::slot_id.
    std::uint32_t slot_id = 0;
    // The shop NPC's Creature::instance_id.
    std::uint32_t creature_instance_id = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
