#include "StoreOpenSucc.h"

#include "common/PayloadWriter.h"

void BankItemSlot::Serialize(PayloadWriter& writer) const
{
    writer.Write(item_id);
    writer.Write(qty_or_refine);
    writer.Write(option_bits);
}

void StoreOpenSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(unknown_header);

    for (const auto& slot : slots)
    {
        slot.Serialize(writer);
    }

    writer.Write(unknown_a);
    writer.Write(unknown_b);
}
