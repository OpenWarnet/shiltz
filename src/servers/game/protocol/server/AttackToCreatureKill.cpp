#include "AttackToCreatureKill.h"

#include "common/PayloadWriter.h"

void AttackToCreatureKill::Serialize(PayloadWriter& writer) const
{
    writer.Write(target_instance_id);
    writer.Write(target_damage);
    writer.Write(target_hp);
    writer.Write(exp_gain);
    writer.Write(unknown_1);
    writer.Write(unknown_2);
    writer.Write(display_damage);
}
