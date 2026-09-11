#pragma once

#include "Bank.h"
#include "Character.h"
#include "common/ConnectionId.h"

#include <cstdint>
#include <optional>
#include <vector>

// The human behind a connection -- as opposed to the Character they're
// playing on screen (see Character.h).
struct Player
{
    ConnectionId connection = 0;

    // Login server's session id, as sent in CG_ENTER (GameEnter::session_id).
    std::uint32_t session_id = 0;

    std::int64_t account_id = 0;

    Character character;

    // Set from CG_STORE_OPEN until CG_STORE_CLOSE or leaving.
    std::optional<Bank> bank;

    // Instance ids of the other characters this client has loaded, sorted; kept mutual by
    // MovementSystem. A sorted vector rather than a set so moving a Player stays noexcept --
    // Pool's vector deep-copies every Player on growth otherwise.
    std::vector<std::uint32_t> visible_players;
};
