#include "ItemConfirmNpcSucc.h"

#include "common/PayloadWriter.h"

void ItemConfirmNpcResult::Serialize(PayloadWriter& writer) const
{
    writer.Write(slot_id);
    writer.Write(option_bits);
}

void ItemConfirmNpcSucc::Serialize(PayloadWriter& writer) const
{
    for (const auto& result : results)
    {
        result.Serialize(writer);
    }

    writer.Write(static_cast<std::uint32_t>(results.size()));
    writer.Write(total_fee);
}
