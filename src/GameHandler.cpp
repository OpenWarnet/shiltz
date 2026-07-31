#include "GameHandler.h"

#include "GamePacket.h"
#include "common/PayloadReader.h"
#include "common/PayloadWriter.h"
#include "models/CharExitSucc.h"
#include "models/CharacterDataLoad.h"
#include "models/CrtLoad.h"
#include "models/GameEnter.h"
#include "models/InventoryItemList.h"

#include <ctime>
#include <iomanip>
#include <iostream>

GameHandler::GameHandler(SOCKET clientSocket, std::span<const uint8_t> key)
    : m_clientSocket(clientSocket), m_key(key)
{
}

bool GameHandler::Handle(GamePacket packet)
{
    if (packet.GetCode() == 411005) // CG_ENTER
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CG_ENTER packet with payload size: " << payload.size() << "\n";

        PayloadReader reader(payload);
        GameEnter request;
        if (!request.Deserialize(reader))
        {
            std::cout << "Failed parsing request\n";
        }

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

        GamePacket responsePacket(511001, data); // GC_CHAR_DATA_LOAD
        auto responsePayload = responsePacket.Serialize(m_key);

        send(m_clientSocket, reinterpret_cast<const char*>(responsePayload.data()),
             static_cast<int>(responsePayload.size()), 0);

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

        GamePacket inventoryPacket(511591, inventoryData); // GC_INVENTORY_ITEM_LIST
        auto inventoryPayload = inventoryPacket.Serialize(m_key);

        send(m_clientSocket, reinterpret_cast<const char*>(inventoryPayload.data()),
             static_cast<int>(inventoryPayload.size()), 0);

        PayloadWriter crtLoadWriter;
        CrtLoad crtLoadResponse{
            .records = {},
        };
        crtLoadResponse.Serialize(crtLoadWriter);
        auto crtLoadData = crtLoadWriter.Data();

        GamePacket crtLoadPacket(511029, crtLoadData); // GC_CRT_LOAD
        auto crtLoadPayload = crtLoadPacket.Serialize(m_key);

        send(m_clientSocket, reinterpret_cast<const char*>(crtLoadPayload.data()),
             static_cast<int>(crtLoadPayload.size()), 0);

        return true;
    }
    else if (packet.GetCode() == 412039) // CG_PLAY_START
    {
        // Fire-and-forget signal from the client ("I've finished loading and
        // am entering the world") -- confirmed via both the real ggg_all2
        // server binary and the client's own send-site trace that NO packet
        // is sent in response to this. The world-enter burst
        // (GC_CHAR_DATA_LOAD/GC_INVENTORY_ITEM_LIST/GC_CRT_LOAD) belongs to
        // CG_ENTER only -- resending it here made the client re-run its
        // enter-world sequence, which fires CG_PLAY_START again, which
        // resent the burst again, in an infinite loop until the client gave
        // up and disconnected.
        std::cout << "Received CG_PLAY_START packet.\n";
        return true;
    }
    else if (packet.GetCode() == 411007) // CG_EXIT
    {
        // Log-out signal, empty body (confirmed both server- and
        // client-side -- see game/handlers/cg_exit.py). The real server
        // saves the character here (three UPDATE statements: pc/inventory/
        // cash_inventory) before replying; we have no DB backend yet, so
        // this just replies with GC_CHAR_EXIT_SUCC as-is, matching the real
        // capture's CG_EXIT -> immediate GC_CHAR_EXIT_SUCC pairing.
        std::cout << "Received CG_EXIT packet.\n";

        PayloadWriter exitWriter;
        CharExitSucc exitResponse{
            .unused = 0,
        };
        exitResponse.Serialize(exitWriter);
        auto exitData = exitWriter.Data();

        GamePacket exitPacket(522010, exitData); // GC_CHAR_EXIT_SUCC
        auto exitPayload = exitPacket.Serialize(m_key);

        send(m_clientSocket, reinterpret_cast<const char*>(exitPayload.data()),
             static_cast<int>(exitPayload.size()), 0);

        return true;
    }
    else
    {
        std::cout << "Received unknown packet code: " << std::hex << packet.GetCode() << std::dec
                  << "\n";
    }
    return false;
}
