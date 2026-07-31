#include "Auth.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/Login.h"
#include "protocol/server/ServerList.h"

#include <iostream>

void HandleClLogin(const LoginContext& ctx, const Login& login)
{
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

void HandleClUserSystemSpecInfo(const LoginContext&)
{
    std::cout << "Received CL_USER_SYSTEM_SPEC_INFO packet.\n";
}

void HandleClGameguard(const LoginContext&)
{
    std::cout << "Received CL_GAMEGUARD packet.\n";
}
