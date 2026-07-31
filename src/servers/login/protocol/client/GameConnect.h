#pragma once

#include <cstdint>
#include <string>

class PayloadReader;

struct GameConnect
{
    std::uint32_t server_id;
    std::uint32_t channel_id;
    std::string char_name;

    bool Deserialize(PayloadReader& reader);
};
