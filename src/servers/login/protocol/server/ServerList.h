#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PayloadWriter;

struct Server
{
    std::string name;

    std::vector<uint32_t> channel_players{};

    void Serialize(PayloadWriter& writer) const;
};

struct ServerList
{
    std::vector<Server> servers;

    void Serialize(PayloadWriter& writer) const;
};
