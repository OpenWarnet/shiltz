#pragma once

#include "../core/Entity.h"

#include <cstdint>

namespace world_v2
{

// Hit points. `current` reaching 0 does not itself remove anything --
// DeathSystem notices, emits an event, and the barrier is where the entity
// actually goes away. Nothing structural happens mid-sweep.
struct HealthComponent
{
    int current = 0;
    int max = 0;
};

// Who an entity fights for.
//
// Not in the original design document, but AI is meaningless without it:
// with no way to tell friend from foe, the first monster to see another
// monster attacks it. A plain integer is the least-committal thing that
// works -- it covers players versus monsters today and monster factions
// later, without deciding now whether "player" is a type, a team, or a
// guild.
//
// Entities with no FactionComponent are invisible to AI targeting rather
// than universally hostile: an unfactioned entity is one nobody has said
// anything about yet, and picking a fight over it would be a guess.
struct FactionComponent
{
    int faction = 0;
};

// Monster targeting state.
//
// `currentTarget` is an Entity, not a bare id, so it carries a generation:
// if the target dies and its slot is recycled by an unrelated spawn, the
// stale handle fails Registry::Exists instead of silently retargeting
// whatever took its place. Ranges are in tiles, measured as Chebyshev
// distance -- the same metric movement uses, so a diagonal step counts as
// one, and "in range" means the same thing to the AI as to the map.
struct AIComponent
{
    int visionRange = 0;
    int attackRange = 0;
    Entity currentTarget = kNullEntity;
};

// What one attack from this entity takes off. Optional -- AISystem falls
// back to its own default when absent, so a monster needs this only if it
// hits for something other than the default.
struct AttackPowerComponent
{
    int amount = 0;
};

// The entity's own progression.
struct ExperienceComponent
{
    std::uint64_t current = 0;
    std::uint32_t level = 1;
    std::uint64_t requiredForNextLevel = 0;
};

// What killing this entity is worth to whoever lands the last hit.
// Separate from ExperienceComponent, which is the entity's own progress --
// a monster grants experience without earning any.
struct ExperienceRewardComponent
{
    std::uint64_t amount = 0;
};

// Who last damaged this entity. Written by CombatSystem on every hit and
// read by DeathSystem to credit the kill, which is the usual MMO rule:
// last hit takes it.
struct LastAttackerComponent
{
    Entity attacker = kNullEntity;
};

// Marks an entity DeathSystem has already announced.
//
// Death is emitted during the simulation stage but resolved at the barrier,
// so there is a window where a dead entity is still present with zero
// health. Without this tag a second pass would announce the same death
// again -- and since the death handler drops loot and awards experience,
// that is a duplication bug rather than a cosmetic one.
struct DeadComponent
{
};

} // namespace world_v2
