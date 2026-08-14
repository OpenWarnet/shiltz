#include "StoreOpenFail.h"

#include "common/PayloadWriter.h"

void StoreOpenFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
}
