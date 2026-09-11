#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_ITEM_DROP (wire code 411012, c2s) -- drop (part of) an inventory
// stack onto the ground at the character's current position.
struct ItemDrop : ClientProtocol
{
    std::uint32_t slot_id = 0;
    std::uint32_t quantity = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
