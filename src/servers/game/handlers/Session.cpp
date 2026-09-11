#include "Session.h"

#include "GamePacket.h"
#include "Outbox.h"
#include "Persistence.h"
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
#include "world/Zone.h"
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

void HandleConnect(const GameContext& ctx)
{
    ctx.world.Connect(ctx.connection);
}

void HandleEnter(const GameContext& ctx, const GameEnter& request)
{
    auto sendFail = [ctx] { ctx.outbox.Send(ctx.connection, EnterFail{}); };

    ctx.persistence.Run(
        [request](IDatabase& db) { return LoadForEnter(db, request); },
        [ctx, request, sendFail](std::optional<LoadedCharacter> loaded) {
            if (!loaded)
                return sendFail();

            // The client may have disconnected while the character was loading.
            if (!ctx.world.IsConnected(ctx.connection))
                return;

            // Checked before an instance id is handed out, so a rejected duplicate doesn't use one up.
            if (ctx.world.IsOnline(loaded->characterId))
                return sendFail();

            Character& character = loaded->character;
            character.instance_id = EntityIdGenerator::Next();
            RecalculateDerivedStats(character, ctx.data.items, ctx.data.setOptions, ctx.data.statusRates);

            if (!ctx.world.Join(Player{
                    .connection = ctx.connection,
                    .session_id = request.session_id,
                    .account_id = loaded->accountId,
                    .character = character,
                }))
                return sendFail();

            Map* map = ctx.world.GetMap(character.map_id);

            // Arriving is a placement with no previous view, so MovementSystem loads every character and creature in view.
            map->Events().Publish(CharacterZoneChangeEvent{
                .instance_id = character.instance_id,
                .to = Zone::Of(character.x, character.y),
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
        if (reply)
            ctx.outbox.Send(ctx.connection, CharExitSucc{});
    };

    if (request.disconnected)
        ctx.world.Disconnect(ctx.connection);

    // No player: never entered, or already left (a quest warp saves its own destination).
    const std::optional<Player> player = ctx.world.Leave(ctx.connection);
    if (!player)
        return finish();

    // A failed save just leaves the character at its last saved position.
    ctx.persistence.Run([character = player->character](IDatabase& db) { character.SavePosition(db); }, finish,
                        [finish](const std::string&) { finish(); });
}
