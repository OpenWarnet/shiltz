#pragma once

#include <cstdint>

namespace world_v2
{

// The identities that exist above the ECS, where the framework meets the
// thing driving it.
//
// All three are opaque integers on purpose. world_v2 never interprets them
// -- it stores them, compares them, and hands them back. A ConnectionId is
// not a socket, a session, or a pointer to anything; it is the number the
// layer above chose to call a client by, in the same spirit as
// NetworkIDComponent::id being a wire number rather than an Entity. That is
// what lets the simulation route a packet to the right place without
// knowing what a packet, a socket, or a player account is.
//
// Kept in one header rather than defined where each is first needed,
// because Simulation, World, and the component that ties them together all
// have to agree on the exact types, and three separate `using` lines that
// silently drift apart is a bug that compiles.

// Which simulation. World is what maps this onto a game map id, a zone
// name, or whatever the layer above calls it -- down here it is only a key.
using SimulationId = std::uint16_t;

// Never handed out. A Simulation constructed without an explicit id has
// this one, which is correct for a standalone simulation that no World owns
// (every existing test) and detectable if one ever escapes into a World.
inline constexpr SimulationId kInvalidSimulationId = 0;

// Which client. 64-bit because it is expected to be a monotonically
// increasing counter that never reuses a value for the life of the process
// -- reuse is what would let a packet from a reconnected socket land on the
// previous occupant's character.
using ConnectionId = std::uint64_t;

inline constexpr ConnectionId kInvalidConnection = 0;

// Which character the connection is playing. Distinct from ConnectionId
// because the two have different lifetimes: one connection plays a
// succession of characters, and one character is played from a succession
// of connections.
using CharacterId = std::uint32_t;

inline constexpr CharacterId kInvalidCharacter = 0;

} // namespace world_v2
