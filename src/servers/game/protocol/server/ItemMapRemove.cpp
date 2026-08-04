#include "ItemMapRemove.h"

#include "common/PayloadWriter.h"

void ItemMapRemove::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
}
