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

    ctx.sessions.Set(ctx.clientSocket, GameSession{.sessionId = static_cast<int64_t>(request.session_id),
                                                    .accountId = accountId});

    // GameEnter carries no server_id, so this game server instance's own
    // characters are found by (account_id, name) alone.
    auto findCharacter = ctx.db.Prepare(
        "SELECT character.id, character.level, character.job_id, character.gender, "
        "       character.hairstyle_id, character.face_id, "
        "       character.stats_str, character.stats_int, character.stats_dex, "
        "       character.stats_con, character.stats_men, character.stats_sen, "
        "       character_position.map_id, character_position.location_x, "
        "       character_position.location_y "
        "FROM character "
        "JOIN character_position ON character_position.character_id = character.id "
        "WHERE character.account_id = ? AND character.name = ?");
    findCharacter->Bind(0, accountId);
    findCharacter->Bind(1, request.char_name);

    if (!findCharacter->Step())
    {
        std::cout << "Rejecting CG_ENTER: no character '" << request.char_name
                  << "' for this account\n";
        sendFail();
        return;
    }

    const auto characterId = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(0)));
    const auto level = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(1)));
    const auto jobId = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(2)));
    const auto gender = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(3)));
    const auto hairstyleId = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(4)));
    const auto faceId = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(5)));
    const auto statsStr = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(6)));
    const auto statsInt = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(7)));
    const auto statsDex = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(8)));
    const auto statsCon = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(9)));
    const auto statsMen = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(10)));
    const auto statsSen = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(11)));
    const auto mapId = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(12)));
    const auto locX = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(13)));
    const auto locY = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(14)));

    PayloadWriter writer;
    CharacterDataLoad response{
        .self_entity_id = characterId,
        .eps_user_flag = 1, // TODO: no DB column -- kept as the prior hardcoded placeholder
        .map_id = mapId,
        .loc_x = locX,
        .loc_y = locY,
        .level = level,
        .job_id = jobId,
        .gender = gender,
        .current_exp = 100, // TODO: no DB column -- kept as the prior hardcoded placeholder
        .cegel = 9123456,   // TODO: no DB column -- kept as the prior hardcoded placeholder
        .fame = 2556,       // TODO: no DB column -- kept as the prior hardcoded placeholder
        .stats_str = statsStr,
        .stats_int = statsInt,
        .stats_dex = statsDex,
        .stats_con = statsCon,
        .stats_men = statsMen,
        .stats_sen = statsSen,
        .current_hp = 1000, // TODO: no DB column -- kept as the prior hardcoded placeholder
        .current_ap = 500,  // TODO: no DB column -- kept as the prior hardcoded placeholder
        .hair_type = hairstyleId,
        .char_name = request.char_name,
        .record_array_a = {},
        .server_timestamp = static_cast<std::uint32_t>(std::time(nullptr)),
        .face_type = faceId,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket responsePacket(GameOpcode::GC_CHAR_DATA_LOAD, data); // GC_CHAR_DATA_LOAD
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);

    PayloadWriter inventoryWriter;
    InventoryItemList inventoryResponse{.total_count = 0};
    inventoryResponse.slots[0] = {.item_id = 31163, .qty_or_refine = 3};
    inventoryResponse.slots[1] = {.item_id = 1004, .qty_or_refine = 3};
    inventoryResponse.slots[2] = {.item_id = 1005, .qty_or_refine = 6};
    inventoryResponse.slots[3] = {.item_id = 1006, .qty_or_refine = 12};
    inventoryResponse.slots[4] = {.item_id = 1007, .qty_or_refine = 3};
    inventoryResponse.slots[5] = {.item_id = 30547, .qty_or_refine = 6};
    inventoryResponse.slots[6] = {.item_id = 26431, .qty_or_refine = 3};

    inventoryResponse.slots[13] = {.item_id = 200, .qty_or_refine = 5};
    inventoryResponse.slots[14] = {
        .item_id = 112, .qty_or_refine = 0};
    inventoryResponse.slots[15] = {
        .item_id = 109, .qty_or_refine = 7};
    inventoryResponse.slots[16] = {
        .item_id = 434, .qty_or_refine = 0};
    inventoryResponse.slots[17] = {
        .item_id = 25806, .qty_or_refine = 0};

    inventoryResponse.Serialize(inventoryWriter);
    auto inventoryData = inventoryWriter.Data();

    GamePacket inventoryPacket(GameOpcode::GC_INVENTORY_ITEM_LIST, inventoryData); // GC_INVENTORY_ITEM_LIST
    auto inventoryPayload = inventoryPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, inventoryPayload);

    PayloadWriter crtLoadWriter;
    CrtLoad crtLoadResponse{
        .records = {},
    };
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
