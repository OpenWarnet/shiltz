#include "LoginDispatcher.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "common/PayloadReader.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "models/Character.h"
#include "models/CharacterSelection.h"
#include "models/CreateCharacter.h"
#include "models/GameConnect.h"
#include "models/GenericCharacterPayload.h"
#include "models/Login.h"
#include "models/ServerList.h"
#include "models/ServerSelect.h"
#include "models/SetCharacterMap.h"

#include <iomanip>
#include <iostream>

namespace
{
    void HandleClLogin(const LoginContext& ctx, const LoginPacket& packet)
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_LOGIN packet with payload size: " << payload.size() << "\n";

        PayloadReader reader(payload);
        Login login;
        if (!login.Deserialize(reader))
        {
            std::cout << "Failed parsing server selection\n";
        }

        std::cout << "Build: " << login.build << "\n";
        std::cout << "Username: " << login.username << "\n";
        std::cout << "Password: " << login.password << "\n";

        PayloadWriter writer;
        ServerList list{.servers{{.name = "1server", .channel_players{1, 2, 3}}}};
        list.Serialize(writer);
        auto serverData = writer.Data();

        // TODO: Somehow it doesn't matter what I sent, this will not change the Server Select UI.
        LoginPacket responsePacket(LoginOpcode::LC_LOGIN_SUCCESS, serverData);
        auto response = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, response);
    }

    void HandleClUserSystemSpecInfo(const LoginContext&, const LoginPacket&)
    {
        std::cout << "Received CL_USER_SYSTEM_SPEC_INFO packet.\n";
    }

    void HandleClGameguard(const LoginContext&, const LoginPacket&)
    {
        std::cout << "Received CL_GAMEGUARD packet.\n";
    }

    void HandleClGetCharinfo(const LoginContext& ctx, const LoginPacket& packet)
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_GET_CHARINFO packet with payload size: " << payload.size()
                  << "\n";

        PayloadReader reader(payload);
        ServerSelect select;
        if (!select.Deserialize(reader))
        {
            std::cout << "Failed parsing server selection\n";
        }

        std::cout << "Server ID: " << select.server_id << "\n";
        std::cout << "Channel ID: " << select.channel_id << "\n";

        PayloadWriter writer;
        CharacterSelection selection{.server_id = select.server_id,
                                     .char_count = 2,
                                     .char_slot_count = 5,
                                     .characters = {{.name = "Juliet",
                                                     .slot = 1,
                                                     .level = 271,
                                                     .job = 1,
                                                     .appearance =
                                                         {
                                                             .gender = 2,
                                                             .hairStyle = 4,
                                                             .faceStyle = 1,
                                                             .headgear = 31163,
                                                             .headgearRefine = 0,
                                                             .top = 1004,
                                                             .topRefine = 0,
                                                             .bottom = 1005,
                                                             .bottomRefine = 0,
                                                             .shoes = 1006,
                                                             .shoesRefine = 0,
                                                             .weapon = 1007,
                                                             .weaponRefine = 12,
                                                             .shield = 30547,
                                                             .shieldRefine = 6,
                                                             .accessories = 26431,
                                                             .accessoriesRefine = 6,
                                                             .pet = 0,
                                                             .petLevel = 0,
                                                         }},
                                                    {.name = "Arjuna",
                                                     .slot = 3,
                                                     .level = 300,
                                                     .job = 4,
                                                     .appearance = {
                                                         .gender = 1,
                                                         .hairStyle = 4,
                                                         .faceStyle = 1,
                                                         .headgear = 0,
                                                         .headgearRefine = 0,
                                                         .top = 8141,
                                                         .topRefine = 0,
                                                         .bottom = 0,
                                                         .bottomRefine = 0,
                                                         .shoes = 0,
                                                         .shoesRefine = 0,
                                                         .weapon = 0,
                                                         .weaponRefine = 12,
                                                         .shield = 0,
                                                         .shieldRefine = 6,
                                                         .accessories = 0,
                                                         .accessoriesRefine = 6,
                                                         .pet = 0,
                                                         .petLevel = 0,
                                                     }}}};

        selection.Serialize(writer);
        auto charData = writer.Data();

        LoginPacket responsePacket(LoginOpcode::LC_CHARINFO_SUCCESS, charData);
        auto response = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, response);
    }

    void HandleClDeleteCharacter(const LoginContext& ctx, const LoginPacket& packet)
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_DELETE_CHARACTER packet with payload size: " << payload.size()
                  << "\n";

        PayloadReader reader(payload);
        GenericCharacterPayload request;
        if (!request.Deserialize(reader))
        {
            std::cout << "Failed parsing request\n";
        }

        std::cout << "Server ID: " << request.server_id << "\n";
        std::cout << "Character: " << request.char_name << "\n";

        PayloadWriter writer;
        GenericCharacterPayload response{.server_id = request.server_id,
                                         .char_name = request.char_name};
        response.Serialize(writer);
        auto data = writer.Data();

        LoginPacket responsePacket(LoginOpcode::LC_DELETECHAR_SUCCESS, data);
        auto responsePayload = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, responsePayload);
    }

    void HandleClCharDeleteCancle(const LoginContext& ctx, const LoginPacket& packet)
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_CHAR_DELETE_CANCLE packet with payload size: " << payload.size()
                  << "\n";

        PayloadReader reader(payload);
        GenericCharacterPayload request;
        if (!request.Deserialize(reader))
        {
            std::cout << "Failed parsing request\n";
        }

        std::cout << "Server ID: " << request.server_id << "\n";
        std::cout << "Character: " << request.char_name << "\n";

        PayloadWriter writer;
        GenericCharacterPayload response{.server_id = request.server_id,
                                         .char_name = request.char_name};
        response.Serialize(writer);
        auto data = writer.Data();

        LoginPacket responsePacket(LoginOpcode::LC_CHAR_DELETE_CANCLE_SUCCESS, data);
        auto responsePayload = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, responsePayload);
    }

    void HandleClCreateCharacter(const LoginContext& ctx, const LoginPacket& packet)
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_CREATE_CHARACTER packet with payload size: " << payload.size()
                  << "\n";

        PayloadReader reader(payload);
        CreateCharacter request;
        if (!request.Deserialize(reader))
        {
            std::cout << "Failed parsing request\n";
        }

        std::cout << "Server ID: " << request.server_id << "\n";
        std::cout << "Character: " << request.char_name << "\n";
        std::cout << "Map ID: " << request.map_id << " (" << request.loc_x << ", " << request.loc_y
                  << ")\n";
        std::cout << "Job: " << request.job << "\n";
        std::cout << "Gender: " << request.gender << "\n";
        std::cout << "Hairstyle: " << request.hairstyle << "\n";
        std::cout << "Stats: \n";
        std::cout << " - STR " << request.stat_str << "\n";
        std::cout << " - INT " << request.stat_int << "\n";
        std::cout << " - DEX (AGI) " << request.stat_dex << "\n";
        std::cout << " - CON (VIT) " << request.stat_con << "\n";
        std::cout << " - MEN (WIS) " << request.stat_men << "\n";
        std::cout << " - SEN (LUK) " << request.stat_sen << "\n";

        PayloadWriter writer;
        GenericCharacterPayload response{.server_id = request.server_id,
                                         .char_name = request.char_name};
        response.Serialize(writer);
        auto data = writer.Data();

        LoginPacket responsePacket(LoginOpcode::LC_CREATECHAR_SUCCESS, data);
        auto responsePayload = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, responsePayload);
    }

    void HandleClCreateMapNum(const LoginContext& ctx, const LoginPacket& packet)
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_CREATE_MAP_NUM packet with payload size: " << payload.size()
                  << "\n";

        PayloadReader reader(payload);
        SetCharacterMap request;
        if (!request.Deserialize(reader))
        {
            std::cout << "Failed parsing request\n";
        }

        std::cout << "Server ID: " << request.server_id << "\n";
        std::cout << "Character: " << request.char_name << "\n";
        std::cout << "Map ID: " << request.map_id << " (" << request.loc_x << ", " << request.loc_y
                  << ")\n";

        PayloadWriter writer;
        GenericCharacterPayload response{.server_id = request.server_id,
                                         .char_name = request.char_name};
        response.Serialize(writer);
        auto data = writer.Data();

        LoginPacket responsePacket(LoginOpcode::LC_CREATE_MAP_NUM_SUCCESS, data);
        auto responsePayload = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, responsePayload);
    }

    void HandleClGameserverConnect(const LoginContext& ctx, const LoginPacket& packet)
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_GAMESERVER_CONNECT packet with payload size: " << payload.size()
                  << "\n";

        PayloadReader reader(payload);
        GameConnect request;
        if (!request.Deserialize(reader))
        {
            std::cout << "Failed parsing request\n";
        }

        PayloadWriter writer;
        GameConnectSuccess response{
            .server_id = request.server_id,
            .char_name = request.char_name,
            .game_server_ip = "45.58.9.172",
            .game_server_port = 1818,
            .session_id = 479309586,
            .status = 1,
        };
        response.Serialize(writer);
        auto data = writer.Data();

        LoginPacket responsePacket(LoginOpcode::LC_GSERV_CONNECT_SUCCESS, data);
        auto responsePayload = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, responsePayload);
    }
}

LoginDispatcher::LoginDispatcher()
{
    m_handlers[LoginOpcode::CL_LOGIN] = HandleClLogin;
    m_handlers[LoginOpcode::CL_USER_SYSTEM_SPEC_INFO] = HandleClUserSystemSpecInfo;
    m_handlers[LoginOpcode::CL_GAMEGUARD] = HandleClGameguard;
    m_handlers[LoginOpcode::CL_GET_CHARINFO] = HandleClGetCharinfo;
    m_handlers[LoginOpcode::CL_DELETE_CHARACTER] = HandleClDeleteCharacter;
    m_handlers[LoginOpcode::CL_CHAR_DELETE_CANCLE] = HandleClCharDeleteCancle;
    m_handlers[LoginOpcode::CL_CREATE_CHARACTER] = HandleClCreateCharacter;
    m_handlers[LoginOpcode::CL_CREATE_MAP_NUM] = HandleClCreateMapNum;
    m_handlers[LoginOpcode::CL_GAMESERVER_CONNECT] = HandleClGameserverConnect;
}

void LoginDispatcher::Dispatch(const LoginContext& ctx, const LoginPacket& packet) const
{
    auto it = m_handlers.find(packet.GetCode());
    if (it == m_handlers.end())
    {
        std::cout << "Received unknown packet code: " << std::hex << packet.GetCode() << "\n";
        return;
    }

    it->second(ctx, packet);
}
