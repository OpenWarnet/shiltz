#include "Character.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/CreateCharacter.h"
#include "protocol/client/ServerSelect.h"
#include "protocol/client/SetCharacterMap.h"
#include "protocol/server/CharacterSelection.h"
#include "protocol/shared/GenericCharacterPayload.h"

#include <iostream>

void HandleClGetCharinfo(const LoginContext& ctx, const ServerSelect& select)
{
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

void HandleClDeleteCharacter(const LoginContext& ctx, const GenericCharacterPayload& request)
{
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

void HandleClCharDeleteCancle(const LoginContext& ctx, const GenericCharacterPayload& request)
{
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

void HandleClCreateCharacter(const LoginContext& ctx, const CreateCharacter& request)
{
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

void HandleClCreateMapNum(const LoginContext& ctx, const SetCharacterMap& request)
{
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
