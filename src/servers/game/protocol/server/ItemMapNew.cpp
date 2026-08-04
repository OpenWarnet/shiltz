#include "ItemMapNew.h"

#include "common/PayloadWriter.h"

void ItemMapNew::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
    writer.Write(x);
    writer.Write(y);
    writer.Write(item_id);
    writer.Write(owner_id);
}
