#pragma once

#include "common/ConnectionId.h"

class Outbox;
class Persistence;
class World;
class GameData;

struct GameContext
{
    ConnectionId connection;
    World& world;
    const GameData& data;

    const Outbox& outbox;
    Persistence& persistence;
};
