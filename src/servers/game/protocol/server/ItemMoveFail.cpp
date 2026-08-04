#include "ItemMoveFail.h"

#include "common/PayloadWriter.h"

void ItemMoveFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
}
