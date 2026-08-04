#pragma once

#include <cstdint>

class PayloadWriter;

// GC_ITEM_MOVE_FAIL (wire code 532102, s2c) -- rejects CG_ITEM_MOVE. Static
// 4-byte body: a single reason code, no padding.
struct ItemMoveFail
{
    std::uint32_t reason = 1;

    void Serialize(PayloadWriter& writer) const;
};
