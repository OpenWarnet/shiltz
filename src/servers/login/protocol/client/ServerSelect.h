#pragma once

#include <cstdint>

class PayloadReader;

struct ServerSelect
{
    std::uint32_t server_id;
    std::uint32_t channel_id;

    bool Deserialize(PayloadReader& reader);
};
