#include "LevelUpSucc.h"

#include "common/PayloadWriter.h"

void LevelUpSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(level);
    writer.Write(unallocated_stat_points);
    writer.Write(unallocated_sp);
    writer.Write(unallocated_ep);
    writer.Write(current_exp);
}
