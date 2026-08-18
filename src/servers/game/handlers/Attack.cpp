#include "Attack.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "protocol/client/AttackToCrt.h"
#include "protocol/server/AttackToCrtMiss.h"
#include "world/World.h"

// Combat resolution isn't implemented yet -- see Map::AttackCreature's own
// comment on ai_target_id -- so every attack is reported as a miss for
// now. This still interrupts the target's current Idle/Wander action and
// tags it as attacking the requesting player, which is the point of this
// handler until real damage math exists.
void HandleAttackToCrt(const GameContext& ctx, const AttackToCrt& request)
{
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
        return;

    Map* map = ctx.world.GetMap(session->player.map_id);
    if (map)
        map->AttackCreature(request.target_id, session->player.instance_id);

    PayloadWriter writer;
    AttackToCrtMiss response{
        .target_instance_id = request.target_id,
    };
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_ATTACK_TO_CRT_MISS, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
}
