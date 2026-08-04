#include "ItemDropSuccess.h"

#include "common/PayloadWriter.h"

void ItemDropSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
    writer.Write(x);
    writer.Write(y);
    writer.Write(item_id);
    writer.Write(source_slot_id);

    std::uint32_t reservedGap[4]{};
    writer.Write(reservedGap);
}
