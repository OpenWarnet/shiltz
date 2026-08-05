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

    // GameEnter carries no server_id, so this game server instance's own
    // characters are found by (account_id, name) alone.
    auto findCharacter =
        ctx.db.Prepare("SELECT character.id, character.level, character.job_id, character.gender, "
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

    const auto characterId =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(0)));

    ctx.sessions.Set(ctx.clientSocket,
                     GameSession{.sessionId = static_cast<int64_t>(request.session_id),
                                 .accountId = accountId,
                                 .characterId = characterId});

    const auto level = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(1)));
    const auto jobId = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(2)));
    const auto gender = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(3)));
    const auto hairstyleId =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(4)));
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

    auto findEquipment = ctx.db.Prepare(
        "SELECT slot, item_id, refine_level FROM equipment_slot WHERE character_id = ?");
    findEquipment->Bind(0, characterId);

    while (findEquipment->Step())
    {
        const int64_t slot = std::get<int64_t>(findEquipment->Column(0));
        if (slot < 0 || static_cast<std::size_t>(slot) >= InventoryItemList::kBagStartSlot)
        {
            std::cout << "Ignoring equipment_slot row with out-of-range slot " << slot << "\n";
            continue;
        }

        const SqlValue itemIdColumn = findEquipment->Column(1);
        if (!std::holds_alternative<int64_t>(itemIdColumn))
            continue; // NULL item_id -- empty slot, leave the wire slot zeroed

        const SqlValue refineLevelColumn = findEquipment->Column(2);
        const auto refineLevel =
            std::holds_alternative<int64_t>(refineLevelColumn)
                ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                : 0;

        inventoryResponse.slots[static_cast<std::size_t>(slot)] = {
            .item_id = static_cast<std::uint32_t>(std::get<int64_t>(itemIdColumn)),
            .qty_or_refine = refineLevel,
        };
    }

    auto findInventory = ctx.db.Prepare("SELECT slot_index, item_id, quantity, refine_level FROM "
                                        "inventory_slot WHERE character_id = ?");
    findInventory->Bind(0, characterId);

    while (findInventory->Step())
    {
        const int64_t slotIndex = std::get<int64_t>(findInventory->Column(0));
        const int64_t wireSlot = static_cast<int64_t>(InventoryItemList::kBagStartSlot) + slotIndex;
        if (slotIndex < 0 || static_cast<std::size_t>(wireSlot) >= InventoryItemList::kTotalSlots)
        {
            std::cout << "Ignoring inventory_slot row with out-of-range slot_index " << slotIndex
                      << "\n";
            continue;
        }

        // If the item has a refine_level, that's what it is -- used as-is
        // (equippable items' "+N" display). Otherwise it's a stackable
        // item: refine=N-1 displays as "N pcs" (see InventoryItemList.h),
        // so db quantity needs the -1 transform.
        const SqlValue quantityColumn = findInventory->Column(2);
        const SqlValue refineLevelColumn = findInventory->Column(3);

        std::uint32_t qtyOrRefine = 0;
        if (std::holds_alternative<int64_t>(refineLevelColumn))
            qtyOrRefine = static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn));
        else if (std::holds_alternative<int64_t>(quantityColumn) &&
                 std::get<int64_t>(quantityColumn) > 0)
            qtyOrRefine = static_cast<std::uint32_t>(std::get<int64_t>(quantityColumn) - 1);

        inventoryResponse.slots[static_cast<std::size_t>(wireSlot)] = {
            .item_id = static_cast<std::uint32_t>(std::get<int64_t>(findInventory->Column(1))),
            .qty_or_refine = qtyOrRefine,
        };
    }

    inventoryResponse.Serialize(inventoryWriter);
    auto inventoryData = inventoryWriter.Data();

    GamePacket inventoryPacket(GameOpcode::GC_INVENTORY_ITEM_LIST,
                               inventoryData); // GC_INVENTORY_ITEM_LIST
    auto inventoryPayload = inventoryPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, inventoryPayload);

    PayloadWriter crtLoadWriter;
    CrtLoad crtLoadResponse{
        .records =
            {
                CrtLoadRecord{.id = 20828,
                              .x = 200,
                              .y = 200,
                              .monster_id = 5643,
                              .direction = 3,
                              .hp = 108000},
                CrtLoadRecord{.id = 20827,
                              .x = 200,
                              .y = 210,
                              .monster_id = 5631,
                              .direction = 4,
                              .hp = 108000},
                CrtLoadRecord{.id = 20826,
                              .x = 200,
                              .y = 220,
                              .monster_id = 5630,
                              .direction = 4,
                              .hp = 108000},
                CrtLoadRecord{.id = 20825,
                              .x = 200,
                              .y = 230,
                              .monster_id = 983,
                              .direction = 2,
                              .hp = 108000},
                CrtLoadRecord{.id = 20824,
                              .x = 200,
                              .y = 240,
                              .monster_id = 980,
                              .direction = 7,
                              .hp = 108000},
                CrtLoadRecord{.id = 20822,
                              .x = 200,
                              .y = 250,
                              .monster_id = 843,
                              .direction = 2,
                              .hp = 108000},
                CrtLoadRecord{.id = 20811,
                              .x = 200,
                              .y = 260,
                              .monster_id = 618,
                              .direction = 5,
                              .hp = 108000},
                CrtLoadRecord{.id = 20807,
                              .x = 200,
                              .y = 190,
                              .monster_id = 584,
                              .direction = 7,
                              .hp = 108000},
            },
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
