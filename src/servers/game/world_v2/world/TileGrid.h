#pragma once

#include "../core/Entity.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace world_v2
{

// One map's tile layer: what the terrain allows, and who is standing where.
//
// Two flat arrays of width * height, indexed by y * width + x. No quadtree,
// no boundary math, no per-entity distance checks -- "can something stand
// at (x, y)" and "who is at (x, y)" are both one array read. That is the
// whole reason the simulation is locked to integer coordinates.
//
//   m_walkable   terrain. Static once the map is loaded.
//   m_occupant   which entity is standing on each tile, or kNullEntity.
//                Changes constantly, and is the half that can drift.
//
// Occupancy is exclusive: one entity per tile, so creatures block each
// other and a move has to find its destination empty. Things that should
// not block -- ground items, effects -- simply never claim a tile (see
// MapWorld::SpawnPassable); they still have a GridPositionComponent, they
// are just not in this array.
//
// The invariant, and why it needs a chokepoint
// -------------------------------------------
// A blocking entity's GridPositionComponent and its slot in m_occupant are
// two copies of the same fact. Write one without the other and the map
// grows phantom walls where something used to stand, or creatures pile onto
// one tile. Nothing here can enforce that on its own -- it cannot see
// components. So every position change goes through MapWorld or
// GridMovementSystem, which update both together, and nothing else assigns
// a GridPositionComponent directly.
//
// Out-of-bounds coordinates are not an error: they read as unwalkable and
// unoccupied, and writes to them are dropped. Map data and AI wander rolls
// both produce them, and neither is worth a crash.
class TileGrid
{
public:
    // `walkableByDefault` is false so that a grid nobody has loaded terrain
    // into blocks everything. A map whose data failed to load should leave
    // its occupants standing still, not hand them an open field to walk
    // through walls in.
    TileGrid(int width, int height, bool walkableByDefault = false)
        : m_width(width < 0 ? 0 : width)
        , m_height(height < 0 ? 0 : height)
        , m_walkable(static_cast<std::size_t>(m_width) * m_height, walkableByDefault ? std::uint8_t{1} : std::uint8_t{0})
        , m_occupant(static_cast<std::size_t>(m_width) * m_height, kNullEntity)
    {
    }

    int Width() const
    {
        return m_width;
    }

    int Height() const
    {
        return m_height;
    }

    bool InBounds(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < m_width && y < m_height;
    }

    // Terrain only -- says nothing about whether someone is standing there.
    // Use IsFree for "could an entity move here".
    bool IsWalkable(int x, int y) const
    {
        return InBounds(x, y) && m_walkable[Index(x, y)] != 0;
    }

    void SetWalkable(int x, int y, bool walkable)
    {
        if (InBounds(x, y))
        {
            m_walkable[Index(x, y)] = walkable ? std::uint8_t{1} : std::uint8_t{0};
        }
    }

    // kNullEntity for an empty tile, and for anything out of bounds.
    Entity OccupantAt(int x, int y) const
    {
        return InBounds(x, y) ? m_occupant[Index(x, y)] : kNullEntity;
    }

    // Walkable terrain with nobody on it.
    bool IsFree(int x, int y) const
    {
        return IsWalkable(x, y) && m_occupant[Index(x, y)] == kNullEntity;
    }

    // Claims (x, y) for `entity`. Fails if the terrain blocks it or someone
    // else is already there; succeeds trivially if `entity` already holds
    // the tile.
    bool TryPlace(Entity entity, int x, int y)
    {
        if (!IsWalkable(x, y))
        {
            return false;
        }

        const Entity occupant = m_occupant[Index(x, y)];
        if (occupant != kNullEntity && occupant != entity)
        {
            return false;
        }

        m_occupant[Index(x, y)] = entity;
        return true;
    }

    // Clears (x, y) only if `entity` is what is actually standing there.
    //
    // The check matters: a passable entity never claimed a tile, so
    // despawning one must not evict the creature that happens to share its
    // coordinates. Same for any handle that has drifted out of date.
    void Remove(Entity entity, int x, int y)
    {
        if (InBounds(x, y) && m_occupant[Index(x, y)] == entity)
        {
            m_occupant[Index(x, y)] = kNullEntity;
        }
    }

    // Unconditional clear. Prefer Remove -- this does not check who it is
    // evicting.
    void Vacate(int x, int y)
    {
        if (InBounds(x, y))
        {
            m_occupant[Index(x, y)] = kNullEntity;
        }
    }

    // Vacates the old tile and claims the new one, or does neither.
    //
    // The single place occupancy changes during movement, so the two halves
    // can never half-apply: if the destination is blocked, the entity keeps
    // the tile it was on. Returns false when the target is out of bounds,
    // unwalkable, or held by someone else.
    bool TryMove(Entity entity, int fromX, int fromY, int toX, int toY)
    {
        if (!IsWalkable(toX, toY))
        {
            return false;
        }

        const Entity occupant = m_occupant[Index(toX, toY)];
        if (occupant != kNullEntity && occupant != entity)
        {
            return false;
        }

        // Ordered so that a move onto the tile already held (from == to)
        // ends up claimed rather than cleared.
        Remove(entity, fromX, fromY);
        m_occupant[Index(toX, toY)] = entity;
        return true;
    }

private:
    std::size_t Index(int x, int y) const
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x);
    }

    int m_width;
    int m_height;

    std::vector<std::uint8_t> m_walkable;
    std::vector<Entity> m_occupant;
};

} // namespace world_v2
