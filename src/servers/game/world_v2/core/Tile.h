#pragma once

#include "Entity.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace world_v2
{

// One map's tile layer: what the terrain allows, and who is standing where.
//
// Two arrays of width * height, indexed by y * width + x. No quadtree, no
// boundary math, no per-entity distance checks -- "can something stand at
// (x, y)" and "who is at (x, y)" are both one array read. That is the whole
// reason the simulation is locked to integer coordinates.
//
//   m_walkable   terrain. Static once the map is loaded.
//   m_occupants  everything standing on each tile. Changes constantly, and
//                is the half that can drift.
//   m_counts     how many are on each tile. Derived from m_occupants, and
//                kept only because the scan needs a dense probe -- see
//                below.
//
// Occupancy is shared, not exclusive
// ----------------------------------
// A tile holds any number of entities. Creatures walk through each other,
// a whole party stands on one square, and a monster can be standing on the
// potion it is about to be killed over. Nothing here refuses a placement
// because something is already there.
//
// This is a deliberate reversal. An earlier version of this file kept one
// Entity per tile and treated the occupancy array as a collision test,
// which made movement and spawning fail in ways the game does not actually
// want: monsters shoving each other out of a corridor, a spawn request
// dropped because the camp was crowded, players unable to stack. The array
// is an *index* -- "what is at (x, y)", answered in O(1) -- and it was
// never a good collision primitive. Terrain is the only thing that blocks.
//
// So the cost model changed with it. A tile lookup is no longer a single
// read but a walk over a short list, and a vision scan is bounded by area
// times local density rather than by area alone. In a crowd that is more
// work; it is still bounded by the scan window rather than by the map's
// whole population, which is the property the index exists for.
//
// Why there is a separate count array
// -----------------------------------
// m_counts holds nothing m_occupants does not already know, which normally
// argues against keeping it -- a derived number that is stored can drift,
// and the rest of this framework prefers recomputing (see SpawnSystem's
// aliveCount). It is here because the measurement said so.
//
// The AI vision scan reads every tile in a square window and, on most of
// them, finds nothing. Under the old exclusive grid that probe was one
// 4-byte Entity out of a packed array -- a linear, cache-friendly sweep.
// Asking a std::vector "are you empty" instead means touching a 24-byte
// header per tile, six times the memory traffic, and the benchmark put the
// cost at roughly +80% on every vision range:
//
//     vision 4  (9x9)     76 us -> 137 us
//     vision 8  (17x17)  313 us -> 549 us
//
// A dense 4-byte count per tile restores the old sweep: the scan reads
// counts, skips the empty tiles without ever touching a vector, and only
// follows the list where something actually is. That brought both rows
// back to within 5% of the exclusive grid -- 76 -> 78 and 313 -> 325.
//
// The order of the two tests in AISystem::FindNearest is part of the fix
// and not a detail: probing emptiness before computing distance was worth
// 40%, because the distance arithmetic is otherwise paid on every empty
// tile the probe was about to discard.
//
// The duplication is confined to this class and to the three functions
// that mutate a tile, and Scenario.test.cpp audits the pair every tick.
//
// The invariant, and why it needs a chokepoint
// --------------------------------------------
// An entity's GridPositionComponent and its membership in that tile's list
// are two copies of the same fact. Write one without the other and the map
// grows phantoms -- entities the AI can see at a tile they left, or a
// creature standing somewhere nothing can find it. Nothing here can enforce
// that on its own, since it cannot see components. So every position change
// goes through Map or GridMovementSystem, which update both together,
// and nothing else assigns a GridPositionComponent directly.
//
// Out-of-bounds coordinates are not an error: they read as unwalkable and
// empty, and writes to them are dropped. Map data and AI wander rolls both
// produce them, and neither is worth a crash.
class Tile
{
public:
    // `walkableByDefault` is false so that a grid nobody has loaded terrain
    // into blocks everything. A map whose data failed to load should leave
    // its occupants standing still, not hand them an open field to walk
    // through walls in.
    Tile(int width, int height, bool walkableByDefault = false)
        : m_width(width < 0 ? 0 : width)
        , m_height(height < 0 ? 0 : height)
        , m_walkable(static_cast<std::size_t>(m_width) * m_height, walkableByDefault ? std::uint8_t{1} : std::uint8_t{0})
        , m_occupants(static_cast<std::size_t>(m_width) * m_height)
        , m_counts(static_cast<std::size_t>(m_width) * m_height, 0)
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

    // Terrain only. Since nothing else blocks, this is now the whole answer
    // to "could an entity move here".
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

    // Everything standing at (x, y), in the order it arrived. Empty for a
    // bare tile and for anything out of bounds.
    //
    // Arrival order is stable and reproducible, which is what lets AI
    // targeting break ties by scan order and still give the same answer on
    // two runs of the same world.
    const std::vector<Entity>& OccupantsAt(int x, int y) const
    {
        static const std::vector<Entity> kNobody;
        return InBounds(x, y) ? m_occupants[Index(x, y)] : kNobody;
    }

    // The dense probe. Reads one 4-byte counter out of a packed array
    // rather than a vector header, which is what keeps a vision scan over
    // mostly-empty tiles as cheap as it was under exclusive occupancy.
    // Check this before OccupantsAt in any loop that sweeps tiles.
    std::size_t OccupantCount(int x, int y) const
    {
        return InBounds(x, y) ? m_counts[Index(x, y)] : 0;
    }

    bool Contains(Entity entity, int x, int y) const
    {
        const std::vector<Entity>& occupants = OccupantsAt(x, y);
        return std::find(occupants.begin(), occupants.end(), entity) != occupants.end();
    }

    // Adds `entity` to (x, y). Fails only if the tile is off the map.
    //
    // Terrain is deliberately not consulted. Placement is authoring -- a
    // corpse's loot lands where the corpse fell, which may well be against
    // a wall -- while walking onto a tile is gameplay, and that is the rule
    // Move enforces. Placing an entity that is already there is a no-op
    // rather than a duplicate, so the tile list never holds one entity
    // twice.
    bool Place(Entity entity, int x, int y)
    {
        if (!InBounds(x, y))
        {
            return false;
        }

        std::vector<Entity>& occupants = m_occupants[Index(x, y)];
        if (std::find(occupants.begin(), occupants.end(), entity) == occupants.end())
        {
            occupants.push_back(entity);
            ++m_counts[Index(x, y)];
        }

        return true;
    }

    // Takes `entity` off (x, y), leaving everything else on it alone.
    //
    // A no-op if it was not there, which is what makes it safe to call with
    // a handle that has drifted out of date.
    void Remove(Entity entity, int x, int y)
    {
        if (!InBounds(x, y))
        {
            return;
        }

        std::vector<Entity>& occupants = m_occupants[Index(x, y)];
        const auto found = std::find(occupants.begin(), occupants.end(), entity);
        if (found != occupants.end())
        {
            // Erase rather than swap-and-pop: the lists are short, and
            // preserving arrival order keeps AI tie-breaking predictable.
            occupants.erase(found);
            --m_counts[Index(x, y)];
        }
    }

    // Clears the tile entirely. Prefer Remove -- this evicts everyone,
    // without asking who they are.
    void Vacate(int x, int y)
    {
        if (InBounds(x, y))
        {
            m_occupants[Index(x, y)].clear();
            m_counts[Index(x, y)] = 0;
        }
    }

    // Takes `entity` off its old tile and puts it on the new one, or does
    // neither.
    //
    // The single place occupancy changes during movement, so the two halves
    // can never half-apply. Returns false when the destination is out of
    // bounds or the terrain blocks it -- the only two things that can stop
    // a step now. Whoever is already standing there is irrelevant.
    bool Move(Entity entity, int fromX, int fromY, int toX, int toY)
    {
        if (!IsWalkable(toX, toY))
        {
            return false;
        }

        // Ordered so that a move onto the tile already held (from == to)
        // ends up present rather than removed.
        Remove(entity, fromX, fromY);
        return Place(entity, toX, toY);
    }

private:
    std::size_t Index(int x, int y) const
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x);
    }

    int m_width;
    int m_height;

    std::vector<std::uint8_t> m_walkable;
    std::vector<std::vector<Entity>> m_occupants;

    // Mirrors m_occupants[i].size(). Maintained by Place, Remove and Vacate
    // -- the only three functions that change a tile -- so the pair cannot
    // drift without one of them being wrong.
    std::vector<std::uint32_t> m_counts;
};

} // namespace world_v2
