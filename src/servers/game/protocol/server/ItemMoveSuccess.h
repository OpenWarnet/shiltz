#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// CG_ITEM_MOVE_SUCC (wire code 521037, s2c) -- acknowledges a successful
// CG_ITEM_MOVE. 2 x u32: the same source_slot_id/dest_slot_id from the
// request.
struct ItemMoveSuccess : ServerMessage<GameOpcode::CG_ITEM_MOVE_SUCC>
{
    std::uint32_t source_slot_id = 0;
    std::uint32_t dest_slot_id = 0;

    void Serialize(PayloadWriter& writer) const override;
};
