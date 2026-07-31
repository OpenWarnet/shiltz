#include "GameHandover.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/GameConnect.h"
#include "protocol/server/GameConnectSuccess.h"

void HandleClGameserverConnect(const LoginContext& ctx, const GameConnect& request)
{
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
