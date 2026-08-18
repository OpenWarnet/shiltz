#include "AttackCrt2TargetMiss.h"

#include "common/PayloadWriter.h"

void AttackCrt2TargetMiss::Serialize(PayloadWriter& writer) const
{
    writer.Write(target_id);
    writer.Write(direction);
    writer.Write(pos_x);
    writer.Write(pos_y);
    writer.Write(unknown);
}
