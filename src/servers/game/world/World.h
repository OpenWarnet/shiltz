#pragma once

#include "Map.h"

#include <chrono>
#include <cstdint>
#include <unordered_map>

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
    void Start();
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
    // GameServer's tick thread (see GameServer::RunTickLoop), never from a
    // per-connection thread. Fans out to every loaded Map's own Tick().
    void Tick(std::chrono::milliseconds delta);

private:
    std::unordered_map<std::int64_t, Map> m_maps;
    std::uint32_t m_nextCreatureInstanceId = 10000;
};
