#pragma once

#include <cstdint>
#include <string>

class PayloadReader;
class PayloadWriter;

struct GenericCharacterPayload
{
    std::uint32_t server_id;
    std::string char_name;

    void Serialize(PayloadWriter& writer) const;

    bool Deserialize(PayloadReader& reader);
};
