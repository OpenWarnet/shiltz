#include "LoginHandler.h"

#include "DESCipher.h"
#include "LoginPacket.h"
#include "common/PayloadReader.h"
#include "common/PayloadWriter.h"
#include "data/Character.h"
#include "data/CharacterSelection.h"
#include "data/Login.h"
#include "data/ServerList.h"
#include "data/ServerSelect.h"

#include <iomanip>
#include <iostream>

LoginHandler::LoginHandler(SOCKET clientSocket, std::span<const uint8_t> key)
    : m_clientSocket(clientSocket), m_key(key)
{
}

bool LoginHandler::Handle(LoginPacket packet)
{
    if (packet.GetCode() == 111000) // CL_LOGIN
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

        std::vector<uint8_t> responseBody;
        uint32_t responseLength = 0;
        PayloadWriter writer;
        ServerList list{.servers{{.name = "1server", .channel_players{1, 2, 3}}}};
        list.Serialize(writer);
        auto server_data = writer.Data();

        responseBody.resize(responseBody.size() + server_data.size());
        std::memcpy(responseBody.data() + responseLength, server_data.data(), server_data.size());
        responseLength += server_data.size();

        for (size_t i = 0; i < responseBody.size(); ++i)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(responseBody[i]) << " ";
        }
        std::cout << std::dec;

        // TODO: Somehow it doesn't matter what I sent, this will not change the Server Select UI.
        LoginPacket responsePacket(221001, responseBody); // LC_LOGIN_SUCCESS
        auto response = responsePacket.Serialize(m_key);

        send(m_clientSocket, reinterpret_cast<const char*>(response.data()),
             static_cast<int>(response.size()), 0);

        return true;
    }
    else if (packet.GetCode() == 111050) // CL_USER_SYSTEM_SPEC_INFO
    {
        std::cout << "Received CL_USER_SYSTEM_SPEC_INFO packet.\n";
        return true;
    }
    else if (packet.GetCode() == 111020) // CL_GAMEGUARD
    {
        std::cout << "Received CL_GAMEGUARD packet.\n";
        return true;
    }
    else if (packet.GetCode() == 111003) // CL_GET_CHARINFO
    {
        const auto& payload = packet.GetPayload();
        std::cout << "Received CL_GET_CHARINFO packet with payload size: " << payload.size()
                  << "\n";
        for (size_t i = 0; i < payload.size(); ++i)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(payload[i]) << " ";
        }
        std::cout << "\n";
        std::cout << std::dec;

        PayloadReader reader(payload);
        ServerSelect select;
        if (!select.Deserialize(reader))
        {
            std::cout << "Failed parsing server selection\n";
        }

        std::cout << "Server ID: " << select.server_id << "\n";
        std::cout << "Channel ID: " << select.channel_id << "\n";

        std::vector<uint8_t> responseBody;
        uint32_t responseLength = 0;
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

        auto char_data = writer.Data();
        responseBody.resize(responseBody.size() + char_data.size());
        std::memcpy(responseBody.data() + responseLength, char_data.data(), char_data.size());
        responseLength += char_data.size();

        for (size_t i = 0; i < responseBody.size(); ++i)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(responseBody[i]) << " ";
        }
        std::cout << std::dec;

        LoginPacket responsePacket(221003, responseBody); // LC_CHARINFO_SUCCESS
        auto response = responsePacket.Serialize(m_key);

        send(m_clientSocket, reinterpret_cast<const char*>(response.data()),
             static_cast<int>(response.size()), 0);

        return true;
    }
    else
    {
        std::cout << "Received unknown packet code: " << std::hex << packet.GetCode() << "\n";
    }
    return false;
}