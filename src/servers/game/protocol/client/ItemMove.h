#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_ITEM_MOVE (wire code 411013, c2s) -- move/swap an inventory item
// between two slots.
struct ItemMove : ClientProtocol
{
    std::uint32_t source_slot_id = 0;
    std::uint32_t dest_slot_id = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
