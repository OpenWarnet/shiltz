#include "QuestFail.h"

#include "common/PayloadWriter.h"

void QuestFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(result_code);
}
