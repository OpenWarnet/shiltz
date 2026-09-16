#include "CrtRemove.h"

#include "common/PayloadWriter.h"

void CrtRemove::Serialize(PayloadWriter& writer) const
{
    writer.Write(creature_id);
}
