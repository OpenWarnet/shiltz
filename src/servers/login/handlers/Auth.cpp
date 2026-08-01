#include "Auth.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/Login.h"
#include "protocol/server/LoginFail.h"
#include "protocol/server/ServerList.h"
#include "storage/IDatabase.h"

#include <iostream>

namespace
{
bool EnsureAccount(IDatabase& db, const std::string& username, const std::string& password)
{
    auto select = db.Prepare("SELECT password FROM accounts WHERE username = ?");
    select->Bind(0, username);

    if (select->Step())
    {
        if (std::get<std::string>(select->Column(0)) != password)
        {
            std::cout << "Password mismatch for account '" << username << "'\n";
            return false;
        }
        return true;
    }

    // TODO: In a real server, you'd want to hash the password before storing it
    // TODO: Remove this and implement proper account creation flow later on
    std::cout << "Registering new account '" << username << "'\n";
    auto insert = db.Prepare("INSERT INTO accounts (username, password) VALUES (?, ?)");
    insert->Bind(0, username);
    insert->Bind(1, password);
    insert->Step();

    return true;
}
} // namespace

void HandleClLogin(const LoginContext& ctx, const Login& login)
{
    std::cout << "Build: " << login.build << "\n";
    std::cout << "Username: " << login.username << "\n";

    PayloadWriter writer;
    if (!EnsureAccount(ctx.db, login.username, login.password))
    {
        // Send login fail packet
        LoginFail failure{.reason = 1};
        failure.Serialize(writer);
        auto failData = writer.Data();

        LoginPacket responsePacket(LoginOpcode::LC_LOGIN_FAIL, failData);
        auto response = responsePacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, response);
        return;
    }

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
