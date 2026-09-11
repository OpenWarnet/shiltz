#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_ITEM_MOVE_FAIL (wire code 532102, s2c) -- rejects CG_ITEM_MOVE. Static
// 4-byte body -- not a real reason code, just an echo of the request's
// source_slot_id back to the client.
struct ItemMoveFail : ServerProtocol
{
    std::uint32_t source_slot_id = 0;

    void Serialize(PayloadWriter& writer) const override;
};
