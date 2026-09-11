#include "Movement.h"

#include "protocol/client/CharMove.h"
#include "world/MapEvents.h"
#include "world/Player.h"
#include "world/World.h"

void HandleMovement(const GameContext& ctx, const CharMove& request, Player& player)
{
    Map* map = ctx.world.GetMap(player.character.map_id);
    if (!map)
        return;

    const std::int32_t fromX = player.character.x;
    const std::int32_t fromY = player.character.y;
    player.character.x = static_cast<std::int32_t>(request.x);
    player.character.y = static_cast<std::int32_t>(request.y);
    player.character.direction = static_cast<std::int32_t>(request.move_direction);

    map->Events().Publish(CharacterMoveEvent{
        .instance_id = player.character.instance_id,
        .from_x = fromX,
        .from_y = fromY,
        .to_x = player.character.x,
        .to_y = player.character.y,
        .walk = CharacterWalk{
            .direction = request.move_direction,
            .speed = request.speed,
            .stop_direction = request.stop_direction,
        },
    });
}
