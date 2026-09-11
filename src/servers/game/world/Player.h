#pragma once

#include "Bank.h"
#include "Character.h"
#include "common/ConnectionId.h"

#include <cstdint>
#include <optional>

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
};
