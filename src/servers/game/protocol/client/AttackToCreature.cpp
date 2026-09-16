#include "AttackToCreature.h"

#include "common/PayloadReader.h"
#include "handlers/Combat.h"

bool AttackToCreature::Deserialize(PayloadReader& reader)
{
    return reader.Read(target_instance_id) && reader.Read(direction) && reader.Read(player_x) &&
           reader.Read(player_y) && reader.Read(constant_one) && reader.Read(unknown);
}

void AttackToCreature::Handle(const GameContext& ctx, Player& player) const
{
    HandleAttackToCreature(ctx, *this, player);
}
