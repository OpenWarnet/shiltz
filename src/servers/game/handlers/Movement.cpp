#include "Movement.h"

#include "protocol/client/CharMove.h"
#include "world/Map.h"
#include "world/Player.h"
#include "world/World.h"

void HandleMovement(const GameContext& ctx, const CharMove& request, Player& player)
{
    Map* map = ctx.world.GetMap(player.character.map_id);
    if (!map)
        return;

    map->Move(player, request.x, request.y, request.move_direction, request.speed,
              request.stop_direction);
}
