#include "GameConnect.h"
#include "common/PayloadReader.h"
#include "common/PayloadWriter.h"

bool GameConnect::Deserialize(PayloadReader& reader)
{
    return reader.Read(server_id) && reader.Read(channel_id) && reader.ReadString(char_name, 16);
}

void GameConnectSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(server_id);
    writer.WriteString(char_name, 16);
    writer.WriteString(game_server_ip, 16);
    writer.Write(game_server_port);
    writer.Write(session_id);
    writer.Write(status);
}