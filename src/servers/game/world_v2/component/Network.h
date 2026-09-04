#pragma once

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
// Deliberately holds no socket, connection id, or session handle. world_v2
// filters by position and hands back the viewer *entity*; mapping that to
// whoever is on the other end of a wire is the game layer's job, and the
// reason nothing in here has to know what a connection is.
//
// `radius` is in tiles, measured as Chebyshev distance -- the same metric
// movement and AI use, so "in view" means the same thing everywhere.
struct ViewerComponent
{
    int radius = 0;
};

} // namespace world_v2
