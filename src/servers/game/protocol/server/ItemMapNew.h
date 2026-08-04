#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PayloadWriter;

struct ItemMapNew
{
    std::uint32_t id;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t item_id;
    std::uint32_t owner_id;

    void Serialize(PayloadWriter& writer) const;
};
