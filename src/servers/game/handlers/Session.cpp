#include "Session.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/GameEnter.h"
#include "protocol/server/CharExitSucc.h"
#include "protocol/server/CharacterDataLoad.h"
#include "protocol/server/CrtLoad.h"
#include "protocol/server/EnterFail.h"
#include "protocol/server/InventoryItemList.h"
#include "storage/IDatabase.h"
#include "world/World.h"

#include <ctime>
#include <iostream>

void HandleEnter(const GameContext& ctx, const GameEnter& request)
{
    std::cout << "Session ID: " << request.session_id << "\n";
    std::cout << "Character: " << request.char_name << "\n";
    std::cout << "Username: " << request.username << "\n";
    // Password intentionally not logged.

    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        EnterFail{}.Serialize(failWriter);
        auto failData = failWriter.Data();

        GamePacket failPacket(GameOpcode::GC_ENTER_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    auto findSession = ctx.db.Prepare("SELECT account_id FROM session WHERE id = ?");
    findSession->Bind(0, static_cast<int64_t>(request.session_id));

    if (!findSession->Step())
    {
        std::cout << "Rejecting CG_ENTER: unknown session " << request.session_id << "\n";
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
        std::cout << "Rejecting CG_ENTER: username '" << request.username
                  << "' does not match session's account\n";
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
        std::cout << "Rejecting CG_ENTER: no character '" << request.char_name
                  << "' for this account\n";
        sendFail();
        return;
    }

    const auto characterId = std::get<int64_t>(findCharacterId->Column(0));

    // Must happen before LoadFromDB, so a rejected duplicate login never
    // gets a chance to read/write anything -- see GameSessionStore.h.
    if (!ctx.sessions.TryClaimCharacter(characterId, ctx.clientSocket))
    {
        std::cout << "Rejecting CG_ENTER: character " << characterId
                  << " already has an active session\n";
        sendFail();
        return;
    }

    Player player;
    if (!player.LoadFromDB(ctx.db, characterId))
    {
        std::cout << "Rejecting CG_ENTER: character " << characterId
                  << " vanished between lookup and load\n";
        ctx.sessions.ReleaseCharacterClaim(characterId);
        sendFail();
        return;
    }
    player.known_zones = ctx.world.GetMap().ZonesAround(player.x, player.y);

    GameSession session{
        .sessionId = static_cast<int64_t>(request.session_id),
        .accountId = accountId,
        .characterId = characterId,
        .player = player,
    };
    ctx.sessions.Set(ctx.clientSocket, session);

    PayloadWriter writer;
    CharacterDataLoad response = session.player.ToCharacterDataLoad(
        /*epsUserFlag=*/1, // TODO: no DB column -- kept as the prior hardcoded placeholder
        static_cast<std::uint32_t>(std::time(nullptr)));
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket responsePacket(GameOpcode::GC_CHAR_DATA_LOAD, data); // GC_CHAR_DATA_LOAD
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);

    PayloadWriter inventoryWriter;
    InventoryItemList inventoryResponse = session.player.ToInventoryItemList();
    inventoryResponse.Serialize(inventoryWriter);
    auto inventoryData = inventoryWriter.Data();

    GamePacket inventoryPacket(GameOpcode::GC_INVENTORY_ITEM_LIST,
                               inventoryData); // GC_INVENTORY_ITEM_LIST
    auto inventoryPayload = inventoryPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, inventoryPayload);

    PayloadWriter crtLoadWriter;
    CrtLoad crtLoadResponse;

    for (const auto& [zoneX, zoneY] : session.player.known_zones)
    {
        for (const auto& creature : ctx.world.GetMap().CreaturesInZone(zoneX, zoneY))
        {
            const MonsterRecord* monsterRecord = ctx.world.FindMonsterRecord(creature.monster_id);

            crtLoadResponse.records.push_back(CrtLoadRecord{
                .id = creature.instance_id,
                .x = static_cast<std::uint32_t>(creature.x),
                .y = static_cast<std::uint32_t>(creature.y),
                .monster_id = static_cast<std::uint32_t>(creature.monster_id),
                .direction = static_cast<std::uint32_t>(creature.direction),
                .hp = monsterRecord ? static_cast<std::uint64_t>(monsterRecord->hp) : 0,
            });
        }
    }

    crtLoadResponse.Serialize(crtLoadWriter);
    auto crtLoadData = crtLoadWriter.Data();

    GamePacket crtLoadPacket(GameOpcode::GC_CRT_LOAD, crtLoadData); // GC_CRT_LOAD
    auto crtLoadPayload = crtLoadPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, crtLoadPayload);
}

void HandleCgPlayStart(const GameContext&)
{
    std::cout << "Received CG_PLAY_START packet.\n";
}

void HandleCgExit(const GameContext& ctx)
{
    std::cout << "Received CG_EXIT packet.\n";

    // Release the session/character claim immediately on exit-to-character-
    // select, rather than waiting for the socket to fully disconnect.
    ctx.sessions.Remove(ctx.clientSocket);

    PayloadWriter exitWriter;
    CharExitSucc exitResponse{
        .unused = 0,
    };
    exitResponse.Serialize(exitWriter);
    auto exitData = exitWriter.Data();

    GamePacket exitPacket(GameOpcode::GC_CHAR_EXIT_SUCC, exitData); // GC_CHAR_EXIT_SUCC
    auto exitPayload = exitPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, exitPayload);
}
