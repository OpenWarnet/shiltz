#include "Combat.h"

#include "Persistence.h"
#include "protocol/client/AttackToCreature.h"
#include "repositories/CharacterRepository.h"
#include "storage/IDatabase.h"
#include "world/Combat.h"
#include "world/Creature.h"
#include "world/Map.h"
#include "world/Player.h"
#include "world/World.h"

#include <iostream>

void HandleAttackToCreature(const GameContext& ctx, const AttackToCreature& request, Player& player)
{
    Map* map = ctx.world.GetMap(player.character.map_id);
    if (!map)
        return;

    Creature* target = map->GetCreature(request.target_instance_id);
    if (!target)
        return;

    const std::optional<AttackResult> result =
        player.Attack(*target, BasicAttack{
                                   .direction = request.direction,
                                   .reported_x = request.player_x,
                                   .reported_y = request.player_y,
                               });
    if (!result || result->exp_gain <= 0)
        return;

    // Player::Attack commits EXP in memory with the kill. Persist the same
    // relative credit asynchronously so multiple queued rewards compose.
    const std::int64_t characterId = player.character.id;
    const std::int64_t expGain = result->exp_gain;
    ctx.persistence.Save([characterId, expGain](IDatabase& db)
                         { CharacterRepository::AddExp(db, characterId, expGain); });
}
