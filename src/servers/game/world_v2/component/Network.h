#pragma once

#include "../Ids.h"

#include <cstdint>

namespace world_v2
{

// The id this entity is known by on the wire.
//
// Deliberately not the Entity handle. Entity encodes a storage slot and a
// generation -- an internal detail of how the registry packs memory, which
// must never leak to a client and must never be parsed out of a packet.
// This is the separate, stable, outward-facing number, and the mapping
// between the two is the network layer's job to maintain in both
// directions.
struct NetworkIDComponent
{
    std::uint32_t id = 0;
};

// Marks an entity a client is watching the world through, and how far it
// sees.
//
// Deliberately holds no socket, session handle, or connection id.
// Visibility is a question about position and range, and answering it has
// nothing to do with who is watching: BroadcastSystem filters by
// coordinates and hands back the viewer *entity*, which is why an AI's
// awareness, a spectator, and a logged-in player can all be viewers without
// the system knowing the difference.
//
// The entity -> connection step is PlayerSessionComponent's job, one lookup
// later and only for the viewers that turn out to have one. Keeping the two
// apart is what stops "can this entity see that" from acquiring an opinion
// about networking.
//
// `radius` is in tiles, measured as Chebyshev distance -- the same metric
// movement and AI use, so "in view" means the same thing everywhere.
struct ViewerComponent
{
    int radius = 0;
};

// The connection currently playing this entity, and the character it is
// playing.
//
// This is the entity -> connection half of routing, and it lives on the
// entity rather than in a side table for the reasons the ECS exists: the
// lookup is the same O(1) as any other component, and it is destroyed with
// its entity, so a player who dies or is despawned cannot leave a routing
// entry behind pointing at a recycled slot. Simulation keeps the other half
// -- connection -> Entity -- because that direction has no entity to hang
// off of.
//
// Only players carry this. A monster has a NetworkIDComponent (clients are
// told about it) and no PlayerSessionComponent (nobody is playing it),
// which is exactly the distinction "is this entity somebody" needs, and
// makes `View<PlayerSessionComponent>` the list of players on this map.
//
// Assigned and removed by Simulation's join/leave handling, never by a
// system.
struct PlayerSessionComponent
{
    ConnectionId connection = kInvalidConnection;
    CharacterId character = kInvalidCharacter;
};

} // namespace world_v2
