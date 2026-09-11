#include "CharRemove.h"

#include "common/PayloadWriter.h"

void CharRemove::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
}
