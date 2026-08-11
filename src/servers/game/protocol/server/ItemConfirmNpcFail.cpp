#include "ItemConfirmNpcFail.h"

#include "common/PayloadWriter.h"

void ItemConfirmNpcFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(result_code);
}
