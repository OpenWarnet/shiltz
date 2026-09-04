#pragma once

#include "../core/Entity.h"

#include <cstdint>

namespace world_v2
{

// "This spawner is short one monster."
//
// Deliberately carries no coordinates. Spawning happens at the barrier, and
// a system running in the simulation stage cannot pick tiles for it: until
// something is actually placed, no tile is claimed, so a burst of requests
// filling an empty camp would every one of them choose the same free tile
// and all but the first would fail. The handler picks the tile at the
// moment it places the monster, when the claims in front of it are real.
struct MonsterSpawnRequestEvent
{
    Entity spawner = kNullEntity;
    std::uint32_t templateId = 0;
};

// A monster has been placed on the map. Emitted after the entity exists and
// holds its template's stats, so the network layer can announce it.
struct MonsterSpawnedEvent
{
    Entity entity = kNullEntity;
    Entity spawner = kNullEntity;
    std::uint32_t templateId = 0;
    int x = 0;
    int y = 0;
};

} // namespace world_v2
