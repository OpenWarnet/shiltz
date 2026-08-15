#pragma once

#include "Creature.h"
#include "GroundItem.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

// A single map's simulation state. Ground items stay a flat list (see
// GroundItem.h); creatures (NPCs and monsters, see Creature.h) are indexed by
// which 16x16 zone of the 512x512 world grid their (x, y) falls in, one
// list per zone -- coarse enough that a player's field of view (currently
// the 3x3 zone neighborhood around their own zone, see ZonesAround) is a
// small handful of buckets to gather, without paying for a 512x512 grid of
// mostly-empty per-cell lists.
//
// One Map instance is shared by every connection's thread, so the
// ground-item list is mutex-protected. The creature grid is NOT locked --
// it's populated once, single-threaded, at load and never mutated after.
class Map
{
public:
    static constexpr std::int32_t kGridSize = 512;
    static constexpr std::int32_t kZoneSize = 16;
    static constexpr std::int32_t kZoneGridSize = kGridSize / kZoneSize;

    Map() = default;

    // std::mutex isn't movable, so these can't be defaulted -- move the
    // data, leave each object's own mutex alone. Only used during
    // single-threaded map loading, never while shared across threads.
    Map(Map&& other);
    Map& operator=(Map&& other);
    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;

    void AddItem(GroundItem item);

    // Removes the ground item with this instance id (see GroundItem::id).
    // Returns false if no such item was present (e.g. already picked up).
    bool RemoveItem(std::uint32_t id);

    // Atomically finds and removes the item, returning it if present.
    // Prefer this over RemoveItem to *claim* an item (e.g. pickup) --
    // checking presence and removing as separate calls lets two players
    // racing the same pickup both grab it.
    std::optional<GroundItem> TryTakeItem(std::uint32_t id);

    std::vector<GroundItem> Items() const; // snapshot copy, safe from any thread.

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

    // Advances this map's simulation by `delta`. Called once per world tick
    // from World::Tick, on the tick thread only -- a no-op for now (no
    // respawns/regen/AI exist yet), but this is where that per-map logic
    // will hang once it does.
    void Tick(std::chrono::milliseconds delta);

private:
    mutable std::mutex m_itemsMutex;
    std::vector<GroundItem> m_items;

    std::vector<std::vector<Creature>> m_creatureGrid =
        std::vector<std::vector<Creature>>(static_cast<std::size_t>(kZoneGridSize) * kZoneGridSize);
};
