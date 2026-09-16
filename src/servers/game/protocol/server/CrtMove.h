#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CRT_MOVE (wire code 511032, s2c) -- a monster taking a step, broadcast
// to every player who currently has it in view (see MovementSystem). x/y is
// where it's moving from,
// target_x/target_y is where it's headed -- the client animates the walk
// between the two itself; the server doesn't send a second update once it
// arrives. speed_raw is always 0 for now -- no monster movement-speed stat
// exists yet (see tables/MonsterTable.h). movement_mode is the seventh
// field present in every live capture; observed wandering uses 1.
struct CrtMove : ServerMessage<GameOpcode::GC_CRT_MOVE>
{
    std::uint32_t creature_id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t target_x = 0;
    std::uint32_t target_y = 0;
    std::uint32_t speed_raw = 0;
    std::uint32_t movement_mode = 1;

    void Serialize(PayloadWriter& writer) const override;
};
