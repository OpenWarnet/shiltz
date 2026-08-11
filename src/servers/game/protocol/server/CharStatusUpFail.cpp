#include "CharStatusUpFail.h"

#include "common/PayloadWriter.h"

void CharStatusUpFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(unallocated_point_remaining);
}
