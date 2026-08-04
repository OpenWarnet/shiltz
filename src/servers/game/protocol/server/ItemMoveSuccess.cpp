#include "ItemMoveSuccess.h"

#include "common/PayloadWriter.h"

void ItemMoveSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(source_slot_id);
    writer.Write(dest_slot_id);
}
