#pragma once

#include "../core/Entity.h"

#include <cstdint>

namespace world_v2
{

// What the combat systems announce, for the barrier to resolve.
//
// These are values, not references into the registry: by the time a handler
// runs, the entity a field names may already have been destroyed by an
// earlier handler in the same flush. Handlers check Registry::Exists before
// acting on any handle in here.

// One landed hit, with where the target was standing when it landed.
//
// The coordinates are captured at emit time rather than looked up by the
// handler, and that is not redundancy: the target may be despawned by an
// earlier handler in the same flush, so a later one asking the registry
// where it was would find nothing. Carrying the position makes the event
// independent of which listener was registered first.
struct DamageDealtEvent
{
    Entity attacker = kNullEntity;
    Entity target = kNullEntity;
    int amount = 0;
    int remainingHealth = 0;
    int x = 0;
    int y = 0;
};

// An entity's health reached zero, and where. `killer` is whoever landed
// the last hit, or kNullEntity if the damage had no attributed source.
//
// Same reason for the coordinates as above, and more sharply: the standard
// handler for this event is the one that removes the corpse.
struct DeathEvent
{
    Entity entity = kNullEntity;
    Entity killer = kNullEntity;
    int x = 0;
    int y = 0;
};

// Experience owed to an entity. Emitted rather than applied directly so the
// award lands at the barrier alongside everything else the kill caused.
struct ExperienceAwardEvent
{
    Entity entity = kNullEntity;
    std::uint64_t amount = 0;
};

struct LevelUpEvent
{
    Entity entity = kNullEntity;
    std::uint32_t level = 0;
};

// Something died with a drop table, at these coordinates.
//
// Deliberately unhandled by the default rules: turning a table id into
// actual items needs the .scr game data, which world_v2 knows nothing
// about. This is the seam the game layer listens on. The position travels
// with the event because the corpse is gone by the time it is delivered.
struct LootDropEvent
{
    Entity source = kNullEntity;
    std::uint32_t dropTableId = 0;
    int x = 0;
    int y = 0;
};

} // namespace world_v2
