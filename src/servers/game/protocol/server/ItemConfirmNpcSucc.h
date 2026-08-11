#pragma once

#include <cstdint>
#include <vector>

class PayloadWriter;

struct ItemConfirmNpcResult
{
    std::uint32_t slot_id = 0;
    std::uint32_t option_bits = 0;

    void Serialize(PayloadWriter& writer) const;
};

// GC_ITEM_CONFIRM_NPC_SUCC (wire code 0x081B22, s2c) -- confirms
// CG_ITEM_CONFIRM_NPC_REQUEST. Per-slot [slot_id, option_bits] pairs for
// every slot that actually succeeded (not necessarily every requested
// slot -- see handlers/ItemConfirmNpc.h), followed once by
// [success_count, total_fee].
//
// NOT independently verified against a real client -- a single-item
// (count=1) request has separately been observed on the wire as a fixed
// 20-byte body (status=1, slot_id, option_bits, unknown=0, fee) instead,
// which this layout doesn't produce. Revisit if a real client rejects
// this shape.
struct ItemConfirmNpcSucc
{
    std::vector<ItemConfirmNpcResult> results;
    std::uint32_t total_fee = 0;

    void Serialize(PayloadWriter& writer) const;
};
