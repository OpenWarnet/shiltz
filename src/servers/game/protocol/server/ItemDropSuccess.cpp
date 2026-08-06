#include "ItemDropSuccess.h"

#include "common/PayloadWriter.h"

void ItemDropSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
    writer.Write(x);
    writer.Write(y);
    writer.Write(item_id);
    writer.Write(source_slot_id);
    writer.Write(new_item_id);
    writer.Write(new_item_count);

    std::uint32_t reservedGap[2]{};
    writer.Write(reservedGap);
}
