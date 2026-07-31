#include "GameDispatcher.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "common/OpcodeBinder.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "game/client/GameEnter.h"
#include "game/server/CharExitSucc.h"
#include "game/server/CharacterDataLoad.h"
#include "game/server/CrtLoad.h"
#include "game/server/InventoryItemList.h"

#include <ctime>
#include <iomanip>
#include <iostream>

namespace
{
    auto When(uint32_t opcode) { return OpcodeBinder<GameContext, GamePacket>(opcode); }
}

namespace
{
    void HandleCgEnter(const GameContext& ctx, const GameEnter& request)
    {
        std::cout << "Session ID: " << request.session_id << "\n";
        std::cout << "Character: " << request.char_name << "\n";
        std::cout << "Username: " << request.username << "\n";
        // Password intentionally not logged.

        PayloadWriter writer;
        CharacterDataLoad response{
            .self_entity_id = 1,
            .eps_user_flag = 1,
            .map_id = 124,
            .loc_x = 168,
            .loc_y = 200,
            .level = 271,
            .job_id = 1,
            .gender = 1,
            .current_exp = 100,
            .cegel = 9123456,
            .fame = 2556,
            .stat_attack = 900,
            .stat_magic = 1,
            .stat_defence = 123,
            .stat_eva = 8,
            .stat_hit = 3,
            .current_hp = 1000,
            .current_ap = 500,
            .hair_type = 4,
            .record_array_a = {},
            .server_timestamp = static_cast<std::uint32_t>(std::time(nullptr)),
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
}

GameDispatcher::GameDispatcher()
    : Dispatcher{
          When(GameOpcode::CG_ENTER).ParseAs<GameEnter>().Then(HandleCgEnter),
          When(GameOpcode::CG_PLAY_START).SkipParse(SkipReason::Ignored).Then(HandleCgPlayStart),
          When(GameOpcode::CG_EXIT).SkipParse(SkipReason::Empty).Then(HandleCgExit),
      }
{
}
