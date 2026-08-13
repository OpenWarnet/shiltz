#pragma once

#include <cstdint>
#include <vector>

class PayloadWriter;

struct ItemConfirmNpcResult
{
    std::uint32_t slot_id = 0;
    std::uint64_t option_bits = 0;

    void Serialize(PayloadWriter& writer) const;
};

// GC_ITEM_CONFIRM_NPC_SUCC (wire code 0x081B22, s2c) -- confirms
// CG_ITEM_CONFIRM_NPC_REQUEST.
//
// results.size()==1: a fixed 20-byte body -- status=1, slot_id,
// option_bits, unknown=0, fee. The "unknown" field's role isn't known,
// only that it's always 0.
//
// results.size()>1: uses a speculative per-slot [slot_id, option_bits]
// pair per item, followed once by [success_count, total_fee]. This shape
// hasn't been checked against a multi-slot request.
struct ItemConfirmNpcSucc
{
    std::vector<ItemConfirmNpcResult> results;
    std::uint32_t total_fee = 0;

    void Serialize(PayloadWriter& writer) const;
};
