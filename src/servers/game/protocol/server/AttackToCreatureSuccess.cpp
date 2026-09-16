#include "AttackToCreatureSuccess.h"

#include "common/PayloadWriter.h"

void AttackToCreatureSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(target_instance_id);
    writer.Write(damage);
    writer.Write(target_hp);
    writer.Write(unknown_1);
    writer.Write(attacker_hit_counter);
}
