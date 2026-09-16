#include "AttackPlayerToCreatureSuccess.h"

#include "common/PayloadWriter.h"

void AttackPlayerToCreatureSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(attacker_instance_id);
    writer.Write(direction);
    writer.Write(player_x);
    writer.Write(player_y);
    writer.Write(constant_one);
    writer.Write(target_instance_id);
    writer.Write(damage);
    writer.Write(target_hp);
    writer.Write(attacker_hit_counter);
}
