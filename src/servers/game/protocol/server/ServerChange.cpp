#include "ServerChange.h"

#include "common/PayloadWriter.h"

void ServerChange::Serialize(PayloadWriter& writer) const
{
    writer.WriteString(server_ip, 16);
    writer.Write(session_id);
    writer.Write(server_type);
    writer.Write(channel_id);
    writer.Write(unity_insdn_server_type);
    writer.Write(server_port);
}
