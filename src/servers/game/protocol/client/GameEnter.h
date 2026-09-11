#pragma once

#include "protocol/ClientProtocol.h"

#include <array>
#include <cstdint>
#include <string>

class PayloadReader;

// CG_ENTER (wire code 0x06457d / 411005, c2s) -- sent right after the
// client connects to the assigned game server. Fixed 76-byte body:
// session id (4) + 24 unknown bytes + char_name/username/password
// (16 bytes each) = 76.
struct GameEnter : ClientProtocol
{
    std::uint32_t session_id = 0;
    std::array<std::uint8_t, 24> unknown{};

    std::string char_name;
    std::string username;
    std::string password;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
