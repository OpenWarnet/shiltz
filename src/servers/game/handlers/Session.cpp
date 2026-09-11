#include "Session.h"

#include "GamePacket.h"
#include "GameSessionStore.h"
#include "Outbox.h"
#include "Persistence.h"
#include "common/Server.h"
#include "protocol/client/GameEnter.h"
#include "protocol/client/GameExit.h"
#include "protocol/server/CharExitSucc.h"
#include "protocol/server/EnterFail.h"
#include "storage/IDatabase.h"
#include "tables/GameData.h"
#include "stats/Stats.h"
#include "world/MapEvents.h"
#include "world/Player.h"
#include "world/World.h"
#include "world/common/EntityIdGenerator.h"

#include <optional>
#include <string>

namespace
{
// Everything CG_ENTER needs from the DB; nullopt means reject.
struct LoadedCharacter
{
    std::int64_t accountId = 0;
    std::int64_t characterId = 0;
    Character character;
};

std::optional<LoadedCharacter> LoadForEnter(IDatabase& db, const GameEnter& request)
{
    auto findSession = db.Prepare("SELECT account_id FROM session WHERE id = ?");
    findSession->Bind(0, static_cast<int64_t>(request.session_id));
    if (!findSession->Step())
        return std::nullopt;

    const int64_t accountId = std::get<int64_t>(findSession->Column(0));

    // The session only proves some account logged in, so check it's the one the client claims.
    auto findAccount = db.Prepare("SELECT username FROM accounts WHERE id = ?");
    findAccount->Bind(0, accountId);
    if (!findAccount->Step() || std::get<std::string>(findAccount->Column(0)) != request.username)
        return std::nullopt;

    // GameEnter carries no server_id, so characters are found by (account_id, name) alone.
    auto findCharacterId = db.Prepare("SELECT id FROM character WHERE account_id = ? AND name = ?");
    findCharacterId->Bind(0, accountId);
    findCharacterId->Bind(1, request.char_name);
    if (!findCharacterId->Step())
        return std::nullopt;

    LoadedCharacter loaded{
        .accountId = accountId,
        .characterId = std::get<int64_t>(findCharacterId->Column(0)),
    };
    if (!loaded.character.LoadFromDB(db, loaded.characterId))
        return std::nullopt;

    return loaded;
}
} // namespace

void HandleEnter(const GameContext& ctx, const GameEnter& request)
{
    auto sendFail = [ctx] { ctx.outbox.Send(ctx.clientSocket, EnterFail{}); };

    ctx.persistence.Run(
        [request](IDatabase& db) { return LoadForEnter(db, request); },
        [ctx, request, sendFail](std::optional<LoadedCharacter> loaded) {
            if (!loaded)
                return sendFail();

            // The client may have disconnected while the character was loading.
            if (!ctx.server.IsConnected(ctx.clientSocket))
                return;

            // Checked before an instance id is handed out, so a rejected duplicate doesn't use one up.
            if (ctx.world.IsOnline(loaded->characterId))
                return sendFail();

            Character& character = loaded->character;
            character.instance_id = EntityIdGenerator::Next();
            RecalculateDerivedStats(character, ctx.data.items, ctx.data.setOptions, ctx.data.statusRates);

            // Joined with empty known_zones, so MovementSystem's first CrtLoad covers the whole view.
            if (!ctx.world.Join(Player{
                    .socket = ctx.clientSocket,
                    .session_id = request.session_id,
                    .character = character,
                }))
                return sendFail();

            Map* map = ctx.world.GetMap(character.map_id);

            // Legacy copy for handlers not yet on the map's Player.
            ctx.sessions.Set(ctx.clientSocket, GameSession{
                                                   .sessionId = static_cast<int64_t>(request.session_id),
                                                   .accountId = loaded->accountId,
                                                   .characterId = loaded->characterId,
                                                   .character = character,
                                               });
            map->SetPlayer(ctx.clientSocket, character.x, character.y);

            // Arriving is a placement onto the DB position; MovementSystem sends the creatures in view.
            map->Events().Publish(CharacterMoveEvent{
                .instance_id = character.instance_id,
                .from_x = character.x,
                .from_y = character.y,
                .to_x = character.x,
                .to_y = character.y,
            });
        },
        [sendFail](const std::string&) { sendFail(); });
}

void HandleCgPlayStart(const GameContext&)
{
}

void HandleCgExit(const GameContext& ctx, const GameExit& request)
{
    // Runs after the save attempt; a quick re-login's load is queued behind this save anyway.
    auto finish = [ctx, reply = !request.disconnected]
    {
        ctx.sessions.Remove(ctx.clientSocket);
        if (reply)
            ctx.outbox.Send(ctx.clientSocket, CharExitSucc{});
    };

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
        return finish();

    const std::optional<Player> player = ctx.world.Leave(ctx.clientSocket);
    if (Map* map = ctx.world.GetMap(session->character.map_id))
        map->RemovePlayer(ctx.clientSocket);

    // The map's Player holds the live position; the session copy only if it never spawned.
    const Character character = player ? player->character : session->character;

    // A failed save just leaves the character at its last saved position.
    ctx.persistence.Run([character](IDatabase& db) { character.SavePosition(db); }, finish,
                        [finish](const std::string&) { finish(); });
}
