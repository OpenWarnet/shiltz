#include "TradeSellFail.h"

#include "common/PayloadWriter.h"

void TradeSellFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
}
