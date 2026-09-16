#include "CrtNew.h"

#include "common/PayloadWriter.h"

void CrtNew::Serialize(PayloadWriter& writer) const
{
    record.Serialize(writer);
}
