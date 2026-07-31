#include "GameConnectSuccess.h"
#include "common/PayloadWriter.h"

void GameConnectSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(server_id);
    writer.WriteString(char_name, 16);
    writer.WriteString(game_server_ip, 16);
    writer.Write(game_server_port);
    writer.Write(session_id);
    writer.Write(status);
}
