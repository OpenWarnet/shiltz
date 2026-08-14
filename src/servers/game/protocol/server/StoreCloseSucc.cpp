#include "StoreCloseSucc.h"

#include "common/PayloadWriter.h"

void StoreCloseSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(constant);
}
