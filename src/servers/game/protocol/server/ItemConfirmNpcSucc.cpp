#include "ItemConfirmNpcSucc.h"

#include "common/PayloadWriter.h"

void ItemConfirmNpcResult::Serialize(PayloadWriter& writer) const
{
    writer.Write(slot_id);
    writer.Write(option_bits);
}

void ItemConfirmNpcSucc::Serialize(PayloadWriter& writer) const
{
    if (results.size() == 1)
    {
        // Single-item shape: status, slot_id, option_bits, unknown, fee.
        writer.Write(static_cast<std::uint32_t>(1)); // status
        results[0].Serialize(writer);
        writer.Write(static_cast<std::uint32_t>(0)); // unknown
        writer.Write(total_fee);
        return;
    }

    // Speculative multi-item shape -- see header comment.
    for (const auto& result : results)
    {
        result.Serialize(writer);
    }

    writer.Write(static_cast<std::uint32_t>(results.size()));
    writer.Write(total_fee);
}
