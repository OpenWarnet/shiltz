#pragma once

#include <cstdint>
#include <string>

class PayloadReader;

struct SetCharacterMap
{
    std::string char_name;
    std::uint32_t server_id;
    std::uint32_t map_id; // ID is from mapname.edt
    std::uint32_t loc_x;
    std::uint32_t loc_y;

    bool Deserialize(PayloadReader& reader);
};
