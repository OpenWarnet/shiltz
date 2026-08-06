#pragma once

#include "Creature.h"
#include "Item.h"

#include <cstdint>
#include <utility>
#include <vector>

// A single map's simulation state. Ground items stay a flat list (see
// Item.h); creatures (NPCs and monsters, see Creature.h) are indexed by
// which 16x16 zone of the 512x512 world grid their (x, y) falls in, one
// list per zone -- coarse enough that a player's field of view (currently
// the 3x3 zone neighborhood around their own zone, see ZonesAround) is a
// small handful of buckets to gather, without paying for a 512x512 grid of
// mostly-empty per-cell lists.
class Map
{
public:
    static constexpr std::int32_t kGridSize = 512;
    static constexpr std::int32_t kZoneSize = 16;
    static constexpr std::int32_t kZoneGridSize = kGridSize / kZoneSize;

    void AddItem(Item item);

    // Removes the ground item with this instance id (see Item::id). Returns
    // false if no such item was present (e.g. already picked up).
    bool RemoveItem(std::uint32_t id);

    const std::vector<Item>& Items() const;

    // Places `creature` in the zone its own (x, y) falls in. Silently
    // dropped (with a log line) if that falls outside the 512x512 grid --
    // none of the map data this loads from does today, but a future or
    // corrupt file shouldn't take the server down over it.
    void AddCreature(Creature creature);

    // The creatures in zone (zoneX, zoneY) -- zone coordinates (each unit
    // is kZoneSize world units), not raw (x, y). Empty (not out of bounds)
    // for a zone outside the kZoneGridSize x kZoneGridSize zone grid.
    const std::vector<Creature>& CreaturesInZone(std::int32_t zoneX, std::int32_t zoneY) const;

    // The (up to) 3x3 zone neighborhood of the zone containing (x, y) --
    // that zone plus its up-to-8 neighbors, clipped to the zone grid's
    // bounds. This is a player's current field of view; not yet shaped by
    // facing direction.
    std::vector<std::pair<std::int32_t, std::int32_t>> ZonesAround(std::int32_t x, std::int32_t y) const;

private:
    std::vector<Item> m_items;
    std::vector<std::vector<Creature>> m_creatureGrid =
        std::vector<std::vector<Creature>>(static_cast<std::size_t>(kZoneGridSize) * kZoneGridSize);
};
