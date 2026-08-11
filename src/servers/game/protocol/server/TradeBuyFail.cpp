#include "TradeBuyFail.h"

#include "common/PayloadWriter.h"

void TradeBuyFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(failure);
}
