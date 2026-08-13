#pragma once

#include "Map.h"

#include <cstdint>

// Owns the game server's live simulation state -- for now just a single
// Map. Lifetime is tied to the GameServer process itself (Start()/
// Shutdown() called once each, from GameServer's constructor/destructor),
// NOT to any individual connection or login session -- contrast with
// GameSessionStore, which is per-connection state that comes and goes as
// clients connect and disconnect. The world persists across all of that.
//
// Static, .scr-derived game-data tables (items, monsters, sellers, ...)
// live on GameData, not here -- see tables/GameData.h.
class World
{
public:
    void Start();
    void Shutdown();

    Map& GetMap();
    const Map& GetMap() const;

    // Hands out the next Creature::instance_id, unique across every
    // creature spawned in this World (not per-map -- there's only one
    // World). Starts at 10000, a number clear of anything the .scr data
    // itself uses, and just increments; good enough until creatures can
    // despawn and ids need to be reclaimed.
    std::uint32_t AllocateCreatureInstanceId();

private:
    Map m_map;
    std::uint32_t m_nextCreatureInstanceId = 10000;
};
