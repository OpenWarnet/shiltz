#include "AttackToCrtMiss.h"

#include "common/PayloadWriter.h"

void AttackToCrtMiss::Serialize(PayloadWriter& writer) const
{
    writer.Write(target_instance_id);
    writer.Write(unknown);
}
