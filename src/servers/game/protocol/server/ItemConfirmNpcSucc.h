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

struct ItemConfirmNpcSucc
{
    std::vector<ItemConfirmNpcResult> results;
    std::uint32_t total_fee = 0;

    void Serialize(PayloadWriter& writer) const;
};
