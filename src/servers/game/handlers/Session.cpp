#include "Session.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "protocol/client/GameEnter.h"
#include "protocol/server/CharExitSucc.h"
#include "protocol/server/CharacterDataLoad.h"
#include "protocol/server/CrtLoad.h"
#include "protocol/server/EnterFail.h"
#include "protocol/server/InventoryItemList.h"
#include "storage/IDatabase.h"
#include "tables/GameData.h"
#include "tables/MonsterTable.h"
#include "stats/Stats.h"
#include "simulation/GameSimulation.h"

#include <ctime>

void HandleEnter(const GameContext& ctx, const GameEnter& request)
{
    auto sendFail = [ctx]
    {
        PayloadWriter failWriter;
        EnterFail{}.Serialize(failWriter);
        auto failData = failWriter.Data();

        GamePacket failPacket(GameOpcode::GC_ENTER_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    // Everything below is blocking SQLite work (three lookups plus
    // Player::LoadFromDB) -- run it on the DB pool instead of the
    // connection's reactor thread. request is copied by value so it stays
    // valid once this handler returns; server.SendTo() is safe to call
    // from any thread. The World/Map reads further down are safe to run
    // here too -- Map's state is protected by its own mutexes (or, for the
    // creature grid, safe because it's read-only after load), not confined
    // to any particular thread.
    boost::asio::post(ctx.dbPool, [ctx, request, sendFail]() {
    auto findSession = ctx.db.Prepare("SELECT account_id FROM session WHERE id = ?");
    findSession->Bind(0, static_cast<int64_t>(request.session_id));

    if (!findSession->Step())
    {
        sendFail();
        return;
    }

    const int64_t accountId = std::get<int64_t>(findSession->Column(0));

    // The session only proves "some account logged in and got handed this
    // session_id" -- cross-check it actually belongs to the account the
    // client claims to be, rather than trusting session_id alone.
    auto findAccount = ctx.db.Prepare("SELECT username FROM accounts WHERE id = ?");
    findAccount->Bind(0, accountId);

    if (!findAccount->Step() || std::get<std::string>(findAccount->Column(0)) != request.username)
    {
        sendFail();
        return;
    }

    // GameEnter carries no server_id, so this game server instance's own
    // characters are found by (account_id, name) alone.
    auto findCharacterId =
        ctx.db.Prepare("SELECT id FROM character WHERE account_id = ? AND name = ?");
    findCharacterId->Bind(0, accountId);
    findCharacterId->Bind(1, request.char_name);

    if (!findCharacterId->Step())
    {
        sendFail();
        return;
    }

    const auto characterId = std::get<int64_t>(findCharacterId->Column(0));

    // Must happen before LoadFromDB, so a rejected duplicate login never
    // gets a chance to read/write anything -- see GameSessionStore.h.
    if (!ctx.sessions.TryClaimCharacter(characterId, ctx.clientSocket))
    {
        sendFail();
        return;
    }

    Player player;
    if (!player.LoadFromDB(ctx.db, characterId))
    {
        ctx.sessions.ReleaseCharacterClaim(characterId);
        sendFail();
        return;
    }
    RecalculateDerivedStats(player, ctx.data.items, ctx.data.setOptions, ctx.data.statusRates);

    GameSession session{
        .sessionId = static_cast<int64_t>(request.session_id),
        .accountId = accountId,
        .characterId = characterId,
        .player = player,
    };
    ctx.sessions.Set(ctx.clientSocket, session);

    // Hand the character to the simulation, which is authoritative for its
    // position from here on. The saved position is taken on trust exactly
    // once, right here; every CG_MOVE after this is a request the
    // simulation decides. A refusal means the saved position is off the
    // map -- the enter still succeeds, but movement will be pinned, which
    // is better than silently relocating someone's character.
    ctx.simulation.Join(ctx.clientSocket, player.map_id, player.x, player.y, player.instance_id,
                        static_cast<float>(player.stats.derived.movement_speed));

    PayloadWriter writer;
    CharacterDataLoad response = session.player.ToCharacterDataLoad(
        /*epsUserFlag=*/1, // TODO: no DB column -- kept as the prior hardcoded placeholder
        static_cast<std::uint32_t>(std::time(nullptr)));
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket responsePacket(GameOpcode::GC_CHAR_DATA_LOAD, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);

    PayloadWriter inventoryWriter;
    InventoryItemList inventoryResponse = session.player.ToInventoryItemList();
    inventoryResponse.Serialize(inventoryWriter);
    auto inventoryData = inventoryWriter.Data();

    GamePacket inventoryPacket(GameOpcode::GC_INVENTORY_ITEM_LIST, inventoryData);
    auto inventoryPayload = inventoryPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, inventoryPayload);

    // No GC_CRT_LOAD here any more.
    //
    // This used to walk the player's 3x3 zone view and send everything in
    // it. ViewModule does that on the first tick after the join instead,
    // by the same rule it uses every tick afterwards -- the client gets one
    // batch either way, and there is now only one piece of code deciding
    // what a player can see rather than three that had to agree.
    });
}

void HandleCgPlayStart(const GameContext&)
{
}

void HandleCgExit(const GameContext& ctx)
{
    // Player::x/y is kept live by HandleMovement on every CG_MOVE, but
    // character_position is only ever written at character creation (see
    // Player::LoadFromDB) -- persist the session's current position now,
    // before the session (and with it the only in-memory copy of where the
    // character actually is) goes away.
    auto session = ctx.sessions.Get(ctx.clientSocket);

    // SavePosition is a blocking SQLite call -- run it, and the claim release
    // that must only happen once it's durable (see GameSessionStore.h on
    // why the claim guards against two connections racing on the same DB
    // rows), on the DB pool instead of the connection's reactor thread.
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, session]() {
        if (session)
        {
            session->player.SavePosition(ctx.db);
        }

        // Takes the character out of its map's simulation. Safe from this
        // thread -- it only pushes a command.
        ctx.simulation.Leave(ctx.clientSocket);

        // Release the session/character claim immediately on exit-to-character-
        // select, rather than waiting for the socket to fully disconnect.
        ctx.sessions.Remove(ctx.clientSocket);

        PayloadWriter exitWriter;
        CharExitSucc exitResponse{
            .unused = 0,
        };
        exitResponse.Serialize(exitWriter);
        auto exitData = exitWriter.Data();

        GamePacket exitPacket(GameOpcode::GC_CHAR_EXIT_SUCC, exitData);
        auto exitPayload = exitPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, exitPayload);
    });
}
