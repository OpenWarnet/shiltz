#pragma once

#include <memory>
#include <span>

class GameData;
class Map;
class Outbox;

// Common base so World can hold every per-map system in one container
// regardless of concrete type. Adds no behavior -- a system registers its
// own event listeners in its constructor (see EnterSystem, MovementSystem).
class System
{
public:
    using Factory = std::unique_ptr<System> (*)(Map&, const Outbox&, const GameData&);

    virtual ~System() = default;

    // Every concrete system's factory, in registration order -- see System.cpp.
    // World builds one instance per (map, factory) from this; add a system by
    // appending here, not by touching World.
    static std::span<const Factory> Factories();
};
