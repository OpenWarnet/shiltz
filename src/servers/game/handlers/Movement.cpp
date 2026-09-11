#include "Movement.h"

#include "protocol/client/CharMove.h"
#include "world/Map.h"
#include "world/MapEvents.h"
#include "world/Player.h"
#include "world/World.h"
#include "world/Zone.h"

void HandleMovement(const GameContext& ctx, const CharMove& request, Player& player)
{
    Map* map = ctx.world.GetMap(player.character.map_id);
    if (!map)
        return;

    // Off-map targets are dropped.
    if (!Map::IsInBounds(request.x, request.y))
        return;

    const std::uint32_t fromX = player.character.x;
    const std::uint32_t fromY = player.character.y;
    player.character.x = request.x;
    player.character.y = request.y;
    player.character.direction = request.move_direction;

    // Published before the move so the view updates go out ahead of GC_CHAR_MOVE.
    if (Zone::Crossed(fromX, fromY, player.character.x, player.character.y))
    {
        map->Events().Publish(CharacterZoneChangeEvent{
            .instance_id = player.character.instance_id,
            .from = Zone::Of(fromX, fromY),
            .to = Zone::Of(player.character.x, player.character.y),
        });
    }

    map->Events().Publish(CharacterMoveEvent{
        .instance_id = player.character.instance_id,
        .from_x = fromX,
        .from_y = fromY,
        .to_x = player.character.x,
        .to_y = player.character.y,
        .direction = request.move_direction,
        .speed = request.speed,
        .stop_direction = request.stop_direction,
    });
}
