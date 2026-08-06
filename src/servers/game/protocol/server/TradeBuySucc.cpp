#include "TradeBuySucc.h"

#include "common/PayloadWriter.h"

void TradeBuySucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(slot_id);
    writer.Write(item_id);
    writer.Write(new_count);
    writer.Write(option);
    writer.Write(option2);
    writer.Write(money);
}
