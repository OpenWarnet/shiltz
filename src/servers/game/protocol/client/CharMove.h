#pragma once

#include <array>
#include <cstdint>
#include <string>

class PayloadReader;


struct CharMove
{
    std::uint32_t user_id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t direction = 0;
    std::uint32_t speed = 0;

    bool Deserialize(PayloadReader& reader);
};
