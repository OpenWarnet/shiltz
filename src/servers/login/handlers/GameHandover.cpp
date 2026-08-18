#include "GameHandover.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "LoginSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "protocol/client/GameConnect.h"
#include "protocol/server/GameConnectSuccess.h"

#include <iostream>

void HandleGameServerConnection(const LoginContext& ctx, const GameConnect& request)
{
    auto sessionId = ctx.sessions.GetSessionId(ctx.clientSocket);
    if (!sessionId)
    {
        std::cout << "Rejecting CL_GAMESERVER_CONNECT: socket has no session (never logged in)\n";
        return;
    }

    PayloadWriter writer;
    GameConnectSuccess response{
        .server_id = request.server_id,
        .char_name = request.char_name,
        .game_server_ip = "45.58.9.172",
        .game_server_port = 1818,
        .session_id = static_cast<std::uint64_t>(*sessionId),
        .status = 1,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    LoginPacket responsePacket(LoginOpcode::LC_GSERV_CONNECT_SUCCESS, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}
