#pragma once

#include "Map.h"

#include <boost/asio/thread_pool.hpp>

#include <chrono>
#include <cstdint>
#include <unordered_map>

class GameData;

// One Map's contribution to a World::Tick() call -- which map, and every
// creature move/attack on it this tick (see Map::Tick/Map::TickResult).
// GameServer turns these into GC_CRT_MOVE/GC_ATTACK_CRT2TARGET_MISS
// broadcasts; World itself has no idea packets or players exist, only
// that a tick happened.
struct MapTickResult
{
    std::int64_t server_map_id = 0;
    std::vector<Map::CreatureMove> creature_moves;
    std::vector<Map::CreatureAttack> creature_attacks;
};

// Owns the game server's live simulation state -- one Map per
// server_map_id, as registered in world/data/map.scr. Lifetime is tied to
// the GameServer process itself (Start()/Shutdown() called once each, from
// GameServer's constructor/destructor), NOT to any individual connection or
// login session -- contrast with GameSessionStore, which is per-connection
// state that comes and goes as clients connect and disconnect. The world
// persists across all of that.
//
// Static, .scr-derived game-data tables (items, monsters, sellers, ...)
// live on GameData, not here -- see tables/GameData.h. map.scr is the
// exception: it's read here, at World::Start(), purely to know which Maps
// to build and from which files -- its rows aren't kept around as a
// queryable table the way GameData's are.
class World
{
public:
    // `data` is only read while spawning creatures (see MapLoader::Load,
    // currently just its MonsterTable) -- World doesn't keep a reference
    // to it, so its caller (GameServer) is free to load GameData first and
    // pass it in here. Taking the whole GameData rather than just the one
    // table it uses today means Start() doesn't need a new parameter every
    // time spawning grows to read another table.
    void Start(const GameData& data);
    void Shutdown();

    // Looks up the Map for a given server_map_id (Player::map_id and
    // MapRecord::server_map_id's id-space). Returns nullptr if this World
    // never loaded that map (unknown id, or an unused map.scr slot).
    Map* GetMap(std::int64_t serverMapId);
    const Map* GetMap(std::int64_t serverMapId) const;

    // Hands out the next Creature::instance_id, unique across every
    // creature spawned in this World (not per-map -- there's only one
    // World). Starts at 10000, a number clear of anything the .scr data
    // itself uses, and just increments; good enough until creatures can
    // despawn and ids need to be reclaimed.
    std::uint32_t AllocateCreatureInstanceId();

    // Advances the whole simulation by `delta` -- called once per tick from
    // GameServer's tick timer (see GameServer::ScheduleTick), never from a
    // per-connection thread, and never called again until the previous call
    // returns (GameServer serializes calls on its own strand).
    //
    // Maps are independent simulation state -- nothing that happens on one
    // Map can observe or affect another Map (see Map.h) -- so this fans
    // every Map's Tick() out onto m_mapPool and blocks until they've all
    // finished, rather than walking m_maps on the calling thread. That
    // bounds one World tick's cost by the slowest single Map instead of the
    // sum of every Map, and lets it use as many cores as m_mapPool has
    // threads, however many maps are loaded.
    //
    // Each posted task writes only into its own slot of the pre-sized
    // result vector, so gathering results needs no locking of its own --
    // the disjoint writes are already safe, and remaining.wait() below is
    // the one synchronization point that makes reading them back out safe
    // too.
    std::vector<MapTickResult> Tick(std::chrono::milliseconds delta);

private:
    std::unordered_map<std::int64_t, Map> m_maps;
    std::uint32_t m_nextCreatureInstanceId = 10000;

    // Dedicated worker pool for parallel Map::Tick() calls, sized to
    // hardware concurrency by boost::asio's default thread_pool ctor --
    // deliberately separate from the io_context's own ioThreads pool (see
    // Server.h) so a slow map tick can never starve network I/O, and vice
    // versa. Joined in Shutdown().
    boost::asio::thread_pool m_mapPool;
};
