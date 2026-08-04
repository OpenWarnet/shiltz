#include "EnterFail.h"

#include "common/PayloadWriter.h"

void EnterFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
}
