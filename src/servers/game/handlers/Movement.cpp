#include "Movement.h"

#include "GameSessionStore.h"
#include "protocol/client/CharMove.h"
#include "world/MapEvents.h"
#include "world/Player.h"
#include "world/World.h"

void HandleMovement(const GameContext& ctx, const CharMove& request, Player& player)
{
    Map* map = ctx.world.GetMap(player.character.map_id);
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!map || !session)
        return;

    const auto x = static_cast<std::int32_t>(request.x);
    const auto y = static_cast<std::int32_t>(request.y);
    const auto direction = static_cast<std::int32_t>(request.move_direction);

    const std::int32_t fromX = player.character.x;
    const std::int32_t fromY = player.character.y;
    player.character.x = x;
    player.character.y = y;
    player.character.direction = direction;

    // Legacy copy for handlers not yet on the map's Player.
    session->character.x = x;
    session->character.y = y;
    session->character.direction = direction;
    ctx.sessions.Set(ctx.clientSocket, *session);
    map->SetPlayer(ctx.clientSocket, x, y);

    map->Events().Publish(CharacterMoveEvent{
        .instance_id = player.character.instance_id,
        .from_x = fromX,
        .from_y = fromY,
        .to_x = x,
        .to_y = y,
        .walk = CharacterWalk{
            .direction = request.move_direction,
            .speed = request.speed,
            .stop_direction = request.stop_direction,
        },
    });
}
