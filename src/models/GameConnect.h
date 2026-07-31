#pragma once

#include <cstdint>
#include <string>

class PayloadReader;
class PayloadWriter;

struct GameConnect
{
    std::uint32_t server_id;
    std::uint32_t channel_id;
    std::string char_name;

    bool Deserialize(PayloadReader& reader);
};

struct GameConnectSuccess
{
    std::uint32_t server_id;
    std::string char_name;
    std::string game_server_ip;
    std::uint32_t game_server_port;
    std::uint64_t session_id;
    std::uint32_t status;

    void Serialize(PayloadWriter& writer) const;
};
