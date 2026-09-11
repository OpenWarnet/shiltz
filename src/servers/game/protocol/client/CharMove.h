#pragma once

#include "protocol/ClientProtocol.h"

#include <array>
#include <cstdint>
#include <string>

class PayloadReader;


struct CharMove : ClientProtocol
{
    std::uint32_t move_direction = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t speed = 0;
    std::uint32_t stop_direction = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
