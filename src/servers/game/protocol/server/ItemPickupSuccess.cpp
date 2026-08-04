#include "ItemPickupSuccess.h"

#include "common/PayloadWriter.h"

void ItemPickupSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
    writer.Write(slot_id);
    writer.Write(item_id);
    writer.Write(qty_or_refine);

    std::uint32_t reservedGap[4]{};
    writer.Write(reservedGap);
}
