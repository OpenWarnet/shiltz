#pragma once

#include "../core/Entity.h"

#include <cstdint>

namespace world_v2
{

// A spawn point: what to keep alive, where, how many, and how fast to
// replace losses.
//
// Lives on an entity of its own, so spawners get the same pooled storage
// and the same view iteration as everything else. That entity is not on the
// map -- it has no GridPositionComponent and never claims a tile -- so
// nothing can walk into it, target it, or see it. Its area is carried here
// instead.
//
// The counts are maintained by SpawnSystem, not by hand. aliveCount is
// recomputed from the live SpawnedByComponent links every tick rather than
// incremented and decremented as things die: a count that is derived cannot
// drift, and it stays right no matter how a monster left -- killed,
// warped, despawned by an admin command, or removed by a path nobody has
// written yet.
struct SpawnerComponent
{
    // Which monster to make. Meaningless to world_v2 -- it is handed back
    // to the game layer's factory, which knows what .scr row it names.
    std::uint32_t templateId = 0;

    // Square area, centred on (x, y), extending `radius` tiles each way.
    int x = 0;
    int y = 0;
    int radius = 0;

    // How many of this spawner's monsters should be alive at once.
    int desiredCount = 0;

    // Recomputed every tick by SpawnSystem. Not for callers to set.
    int aliveCount = 0;

    // Seconds to wait before replacing a loss. The clock only runs while
    // the spawner is short, and one monster comes back per interval rather
    // than the whole group at once -- so clearing a camp refills gradually
    // instead of the entire pack reappearing together.
    float respawnDelay = 0.0f;
    float timer = 0.0f;

    // Bumped on every spawn, and mixed into the tile choice so successive
    // monsters from one spawner land on different tiles. Deterministic:
    // the same spawner in the same state always picks the same sequence of
    // tiles, which keeps the simulation reproducible.
    std::uint32_t sequence = 0;
};

// Back-link from a spawned monster to the spawner that owns it.
//
// This is what SpawnSystem counts. It is a plain Entity, so a link to a
// spawner that has since been destroyed fails Registry::Exists and is
// simply not counted, rather than incrementing a stranger.
struct SpawnedByComponent
{
    Entity spawner = kNullEntity;
};

} // namespace world_v2
