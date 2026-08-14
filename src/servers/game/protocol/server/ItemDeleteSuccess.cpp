#include "ItemDeleteSuccess.h"

#include "common/PayloadWriter.h"

void ItemDeleteSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(slot_id);

    std::uint32_t reservedGap[7]{};
    writer.Write(reservedGap);
}
