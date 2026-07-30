#pragma once

#include <cstdint>
#include <string>

class PayloadReader;

struct CreateCharacter
{
    std::uint32_t server_id;
    std::string char_name;
    std::uint32_t slot;
    std::uint32_t map_id;
    std::uint32_t loc_x;
    std::uint32_t loc_y;
    std::uint32_t gender;

    std::uint32_t stat_str;
    std::uint32_t stat_int;
    std::uint32_t stat_dex;
    std::uint32_t stat_con;
    std::uint32_t stat_men;
    std::uint32_t stat_sen;

    std::uint32_t hairstyle;
    std::uint32_t job;

    bool Deserialize(PayloadReader& reader);
};
