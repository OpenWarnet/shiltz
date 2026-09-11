#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CHAR_MOVE (wire code 511000, s2c) -- acknowledges every CG_MOVE.
// user_instance_id (same value as CharacterDataLoad::self_entity_id),
// direction (the character's resulting facing -- CharMove::direction), x,
// y, speed (Player's derived movement_speed stat + 300 -- not an echo of
// CharMove::speed, see handlers/Movement.cpp), stop_direction (same value
// as CharMove::direction again -- CG_MOVE has no separate field for this).
// Field order verified against a real-server capture (id, direction, x, y,
// speed, stop_direction) -- a prior x/direction swap here left the client
// with a garbage confirmed x, which silently broke any client-side
// proximity check keyed off it (e.g. the bank-NPC distance gate).
struct CharMoveUpdate : ServerProtocol
{
    std::uint32_t user_instance_id = 0;
    std::uint32_t direction = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t speed = 0;
    std::uint32_t stop_direction = 0;

    void Serialize(PayloadWriter& writer) const override;
};
