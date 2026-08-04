#pragma once

#include "Map.h"

// Owns the game server's simulation state -- for now just a single Map.
// Lifetime is tied to the GameServer process itself (Start()/Shutdown()
// called once each, from GameServer's constructor/destructor), NOT to any
// individual connection or login session -- contrast with GameSessionStore,
// which is per-connection state that comes and goes as clients connect and
// disconnect. The world persists across all of that.
class World
{
public:
    void Start();
    void Shutdown();

    Map& GetMap();
    const Map& GetMap() const;

private:
    Map m_map;
};
