#pragma once

#include "Character.h"

#include <cstdint>
#include <winsock2.h>

// The human behind a connection -- as opposed to the Character they're
// playing on screen (see Character.h).
struct Player
{
    SOCKET socket = INVALID_SOCKET;

    // Login server's session id, as sent in CG_ENTER (GameEnter::session_id).
    std::uint32_t session_id = 0;

    Character character;
};
