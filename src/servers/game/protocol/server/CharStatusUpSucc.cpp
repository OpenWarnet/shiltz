#include "CharStatusUpSucc.h"

#include "common/PayloadWriter.h"

void CharStatusUpSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(stat_id);
    writer.Write(current_stat_point);
    writer.Write(unallocated_point_remaining);
}
