#include "StoreCreateSucc.h"

#include "common/PayloadWriter.h"

void StoreCreateSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(constant);
}
