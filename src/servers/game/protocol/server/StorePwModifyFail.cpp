#include "StorePwModifyFail.h"

#include "common/PayloadWriter.h"

void StorePwModifyFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
}
