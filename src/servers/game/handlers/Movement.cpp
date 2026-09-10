#include "Movement.h"

#include "GameSessionStore.h"
#include "protocol/client/CharMove.h"
#include "simulation/GameSimulation.h"

// CG_MOVE, in full.
//
// This used to be seventy lines: recompute the player's 3x3 zone view,
// diff it against the last one, send GC_CRT_LOAD for creatures in newly
// entered zones and GC_VIEW_REMOVE_ALL for ones left behind, write the
// client's claimed coordinates into the session, and echo them back.
//
// All of it has moved, and to two different places. The view diff belongs
// to the whole world rather than to one packet -- a creature walking
// towards a standing player has to enter that player's view too, and no
// CG_MOVE arrives to notice it -- so ViewModule recomputes it every tick
// for every viewer. And the position is no longer the client's to assert:
// it is a request, resolved against the map by PlayerMoveSystem, and
// acknowledged from inside the tick with wherever the simulation actually
// put the player.
//
// What is left is the translation, which is the only part that was ever
// this handler's job.
void HandleMovement(const GameContext& ctx, const CharMove& request)
{
    if (!ctx.sessions.Get(ctx.clientSocket))
        return;

    ctx.simulation.PushMove(ctx.clientSocket, static_cast<int>(request.x), static_cast<int>(request.y),
                            request.move_direction, request.stop_direction);
}
