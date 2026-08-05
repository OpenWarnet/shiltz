#pragma once

#include <cstdint>

class PayloadWriter;

// GC_CHAR_MOVE (wire code 511000, s2c) -- acknowledges a successful
// CG_MOVE. 5 x u32: user_instance_id (same value as
// CharacterDataLoad::self_entity_id), x, direction (the character's
// resulting facing -- CharMove::end_direction), y, speed (echoes
// CharMove::move_speed).
struct CharMoveUpdate
{
    std::uint32_t user_instance_id = 0;
    std::uint32_t x = 0;
    std::uint32_t direction = 0;
    std::uint32_t y = 0;
    std::uint32_t speed = 0;

    void Serialize(PayloadWriter& writer) const;
};
