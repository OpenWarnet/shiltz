#include "InventoryItemList.h"

#include "common/PayloadWriter.h"

void InventoryItemSlot::Serialize(PayloadWriter& writer) const
{
    writer.Write(item_id);
    writer.Write(qty_or_refine);
    writer.Write(option_bits);
    writer.Write(unknown_tail);
}

void InventoryItemList::Serialize(PayloadWriter& writer) const
{
    writer.Write(total_count);

    for (const auto& slot : slots)
    {
        slot.Serialize(writer);
    }

    // 4 trailing reserved/zero bytes -- see header comment, part of the
    // confirmed 4104-byte body size.
    std::array<std::uint8_t, 4> trailing{};
    writer.Write(trailing);
}
