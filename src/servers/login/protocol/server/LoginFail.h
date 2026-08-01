#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PayloadWriter;

struct LoginFail
{
    std::uint32_t reason;

    void Serialize(PayloadWriter& writer) const;
};
