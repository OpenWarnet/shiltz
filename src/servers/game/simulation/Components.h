#pragma once

#include "world/Creature.h"
#include "world/Item.h"
#include "world_v2/core/Entity.h"

#include <cstdint>

// The game layer's own components.
//
// world_v2 knows about entities, tiles and events; it does not know what a
// monster is, what an item is, or what number the client calls either of
// them. That is all here, and none of it is visible from inside world_v2.
namespace game_sim
{

// The id this entity is known by on the wire.
//
// Deliberately not the world_v2 Entity handle. An Entity is a storage slot
// with a generation, reused the moment something despawns; a network id has
// to stay meaningful to a client that may still be holding it. Keeping them
// separate is what stops a stale client packet from landing on whoever
// inherited the slot.
struct NetworkIdComponent
{
    std::uint32_t id = 0;
};

// A creature placed from npcNN.scr or mNN.scr.
//
// `templateId` joins NpcSpawn::id or MonsterRecord::id depending on `kind`,
// exactly as the v1 Creature::monster_id did -- this carries placement, not
// the template's stats.
struct CreatureComponent
{
    std::int64_t templateId = 0;
    CreatureKind kind = CreatureKind::Monster;
    std::int32_t direction = 0;
};

// Wander state for a monster. NPCs never get one -- they are static
// dialogue/shop/warp entities and never move.
//
// `decisionSeq` bumps once per roll and feeds the decision's seed alongside
// the network id, so no roll for any creature ever repeats. Carried on the
// component rather than in a shared RNG so a tick needs no global state.
struct CreatureAiComponent
{
    CreatureAiState state = CreatureAiState::Idle;
    std::int64_t timerMs = 0;
    std::uint32_t decisionSeq = 0;
};

// An item lying on the ground.
//
// Embeds the whole Item rather than an id and a quantity, so a drop's
// item_level and option_bits survive the ground stage instead of being lost
// between drop and pickup -- the same reason the v1 GroundItem did.
struct MapItemComponent
{
    Item item;
};

// Marks the entity a connected client is playing, and carries what the
// acknowledgement packets need to echo.
struct PlayerIdentityComponent
{
    std::uint32_t instanceId = 0;
    std::uint32_t facing = 0;
};

// How fast this player may travel, in map units per second. Derived from
// PlayerDerivedStats::movement_speed by the caller, because the stat's unit
// scale is a game-data question this layer should not guess at.
struct PlayerSpeedComponent
{
    float unitsPerSecond = 0.0f;
};

// Where a player says it is going. Consumed by PlayerMoveSystem in the tick
// after the packet arrived, and removed whether or not the move is allowed
// -- one request buys exactly one decision, so a rejected move cannot
// re-fire every tick.
struct MoveRequestComponent
{
    int targetX = 0;
    int targetY = 0;
    std::uint32_t direction = 0;
    std::uint32_t stopDirection = 0;
};

// A player reaching for a ground item, by the item's network id. Resolved
// at the barrier, where despawning the item is legal.
struct PickupRequestComponent
{
    std::uint32_t itemNetworkId = 0;

    // The inventory slot the client asked to put it in. Carried through
    // rather than recomputed, because the claim and the database write
    // happen on different sides of the barrier and the request is the only
    // thing that knows it.
    std::uint32_t slotId = 0;
};

} // namespace game_sim
