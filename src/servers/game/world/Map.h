#pragma once

#include "Creature.h"
#include "GroundItem.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <winsock2.h>

// A single map's simulation state. Ground items stay a flat list (see
// GroundItem.h); creatures (NPCs and monsters, see Creature.h) are indexed by
// which 16x16 zone of the 512x512 world grid their (x, y) falls in, one
// list per zone -- coarse enough that a player's field of view (currently
// the 3x3 zone neighborhood around their own zone, see ZonesAround) is a
// small handful of buckets to gather, without paying for a 512x512 grid of
// mostly-empty per-cell lists.
//
// One Map instance is shared by every connection's thread (reading, via
// CreaturesInZone/Items) and one of World's map-pool threads (writing, via
// Tick -- see World::Tick). The ground-item list and the creature grid are
// each protected by their own lock and each hand back snapshot copies, not
// references, so a caller's copy stays valid even if Tick() reindexes the
// grid a moment later. No Map ever reaches into another Map's state, which
// is what lets World::Tick() run every Map's Tick() concurrently in the
// first place.
//
// Map also keeps a roster of who's currently on it (see MapPlayer/
// SetPlayer/RemovePlayer) -- just enough (a socket plus position) to
// answer "who's here" for map-wide broadcasts like GC_CRT_MOVE, kept live
// by the same handlers that already own player position: Session.cpp's
// HandleEnter/HandleCgExit, Movement.cpp's HandleMovement, Quest.cpp's
// ApplyWarp (moving between two Maps), and GameServer::OnClientDisconnected.
// This is deliberately just a socket/x/y roster, not the full GameSession --
// GameSessionStore stays the one place that owns session identity/data.
class Map
{
public:
    static constexpr std::int32_t kGridSize = 512;
    static constexpr std::int32_t kZoneSize = 16;
    static constexpr std::int32_t kZoneGridSize = kGridSize / kZoneSize;

    // One creature's step during a single TickCreature pass -- (from_x,
    // from_y) is where it was at the start of the tick, (to_x, to_y) where
    // it ended up. GameServer turns these into GC_CRT_MOVE broadcasts (see
    // GameServer::BroadcastCreatureMoves); Map itself has no idea packets
    // or players exist.
    struct CreatureMove
    {
        std::uint32_t creature_id = 0;
        std::int32_t from_x = 0;
        std::int32_t from_y = 0;
        std::int32_t to_x = 0;
        std::int32_t to_y = 0;
    };

    // One connected player's presence on this map -- just enough to notify
    // and to run a visibility check against (see GameServer::
    // BroadcastCreatureMoves), not the full GameSession. See SetPlayer.
    struct MapPlayer
    {
        SOCKET socket = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;
    };

    Map() = default;

    // Neither std::mutex nor std::shared_mutex is movable, so these can't
    // be defaulted -- move the data, leave each object's own locks alone.
    // Only used during single-threaded map loading, never while shared
    // across threads.
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
    // Snapshot copy, safe from any thread -- see the class comment on why
    // this doesn't hand back a reference into the grid.
    std::vector<Creature> CreaturesInZone(std::int32_t zoneX, std::int32_t zoneY) const;

    // The (up to) 3x3 zone neighborhood of the zone containing (x, y) --
    // that zone plus its up-to-8 neighbors, clipped to the zone grid's
    // bounds. This is a player's current field of view; not yet shaped by
    // facing direction.
    std::vector<std::pair<std::int32_t, std::int32_t>> ZonesAround(std::int32_t x, std::int32_t y) const;

    // Adds `socket` to this map's roster, or updates its position if
    // already present (an upsert, so the same call serves both CG_ENTER's
    // initial placement and every later CG_MOVE within this map). Call
    // RemovePlayer first when a player *leaves* this map for another one
    // (see Quest.cpp's ApplyWarp) -- SetPlayer alone never removes a
    // player from a map they've warped away from.
    void SetPlayer(SOCKET socket, std::int32_t x, std::int32_t y);

    // Removes `socket` from this map's roster. No-op if it wasn't on this
    // map. Call on CG_EXIT, on disconnect, and on the leaving side of a
    // warp to a different map.
    void RemovePlayer(SOCKET socket);

    // Snapshot copy of every player currently on this map, safe from any
    // thread -- what GameServer::BroadcastCreatureMoves reads instead of
    // asking GameSessionStore to search for "who's on this map".
    std::vector<MapPlayer> Players() const;

    // True if anyone is currently on this map's roster. Cheap occupancy
    // check for World::Tick to decide whether this Map's simulation is
    // worth running this tick, without paying for a full Players() copy
    // just to test emptiness.
    bool HasPlayers() const;

    // Which zone (zoneX, zoneY) contains world position (x, y) -- pure
    // coordinate math, no grid-bounds check (see ZonesAround/InZoneGrid in
    // Map.cpp for that). Exposed so GameServer can classify a
    // CreatureMove's position into a zone the same way ZonesAround does
    // internally, without duplicating the divide.
    static std::pair<std::int32_t, std::int32_t> ZoneOf(std::int32_t x, std::int32_t y);

    // Advances this map's simulation by `delta`. Called once per world tick
    // from World::Tick, on one of World's map-pool threads -- concurrently
    // with every other Map's Tick(), never twice at once for the *same*
    // Map. Currently just runs TickCreature; this is the extension point
    // for respawns/regen once those exist too. Because Maps never
    // interact, none of this ever needs to reach across into another Map.
    // Returns every creature that moved this tick, for World::Tick to hand
    // up to GameServer.
    //
    // World::Tick only calls this for maps where HasPlayers() is true, so a
    // creature's ai_timer simply doesn't count down while its map is empty
    // -- it resumes from wherever it was left once a player returns, since
    // every tick still advances by the same fixed `delta` regardless of how
    // many ticks a map sat skipped.
    std::vector<CreatureMove> Tick(std::chrono::milliseconds delta);

private:
    // Advances every Monster's Idle/Wander state machine by `delta` (NPCs
    // are skipped -- see CreatureAiState). Takes m_creatureGridMutex
    // exclusively for the whole pass: each creature's timer/position is
    // updated in place first, then anyone who wandered into a different
    // zone is moved to that zone's bucket. See Creature.h for the state
    // machine itself and MixDecisionSeed (Map.cpp) for how each decision
    // is rolled. Returns every creature that actually moved (Wander rolls
    // that land back on the same tile don't happen -- see kWanderOffsets --
    // so a Wander roll always produces one).
    std::vector<CreatureMove> TickCreature(std::chrono::milliseconds delta);

    mutable std::mutex m_itemsMutex;
    std::vector<GroundItem> m_items;

    mutable std::shared_mutex m_creatureGridMutex;
    std::vector<std::vector<Creature>> m_creatureGrid =
        std::vector<std::vector<Creature>>(static_cast<std::size_t>(kZoneGridSize) * kZoneGridSize);

    mutable std::mutex m_playersMutex;
    std::unordered_map<SOCKET, MapPlayer> m_players;
};
