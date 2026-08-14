#include "StorePwModifySucc.h"

#include "common/PayloadWriter.h"

void StorePwModifySucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(unknown);
}
