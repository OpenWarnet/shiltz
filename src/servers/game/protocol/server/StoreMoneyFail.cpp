#include "StoreMoneyFail.h"

#include "common/PayloadWriter.h"

void StoreMoneyFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(failure);
}
