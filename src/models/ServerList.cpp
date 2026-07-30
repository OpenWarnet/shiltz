#include "ServerList.h"

#include "common/PayloadWriter.h"

void ServerList::Serialize(PayloadWriter& writer) const
{
    writer.Write(static_cast<uint32_t>(servers.size()));

    for (const auto& server : servers)
    {
        server.Serialize(writer);
    }
}

void Server::Serialize(PayloadWriter& writer) const
{
    writer.WriteString(name, 16);
    writer.Write(static_cast<uint32_t>(channel_players.size()));

    for (const auto& c : channel_players)
    {
        writer.Write(c);
    }
}