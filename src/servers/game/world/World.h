#pragma once

#include "Atlas.h"
#include "common/ConnectionId.h"
#include "world/common/BatchQueue.h"
#include "world/common/Request.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class EnterSystem;
class MovementSystem;
class GameData;
class Outbox;
struct Player;

class World
{
public:
    World();
    ~World();

    void Start(const Outbox& outbox, const GameData& data);
    void Shutdown();

    Map* GetMap(std::int64_t id);
    const Map* GetMap(std::int64_t id) const;

    // Connections are tracked from GameConnect to the disconnect's GameExit. World strand only.
    void Connect(ConnectionId connection);
    void Disconnect(ConnectionId connection);
    bool IsConnected(ConnectionId connection) const;

    // Spawns player on character.map_id; false if the map isn't loaded, the character is online, or the connection is in.
    [[nodiscard]] bool Join(Player player);

    // Despawns the connection's player and hands it back; nullopt if it has none.
    std::optional<Player> Leave(ConnectionId connection);

    // nullptr if the connection has no player in the world. World strand only.
    Player* FindPlayer(ConnectionId connection);

    // True while a player for this `character` row is in the world.
    bool IsOnline(std::int64_t characterId) const;

    void Receive(Request request);
    void Tick(std::chrono::milliseconds delta);

private:
    // Where a connection's player is, so it can be found without scanning maps.
    struct PlayerRef
    {
        std::int64_t mapId = 0;
        std::uint32_t instanceId = 0;
        std::int64_t characterId = 0;
    };

    std::unordered_set<ConnectionId> m_connections;
    std::unordered_map<ConnectionId, PlayerRef> m_playersByConnection;
    std::unordered_map<std::int64_t, ConnectionId> m_connectionsByCharacter;

    BatchQueue<Request> ingress;

    std::vector<Request> m_requests;
    Atlas m_atlas;

    // One of each per map; heap-allocated because each map's event bus points at them.
    std::vector<std::unique_ptr<EnterSystem>> m_enterSystems;
    std::vector<std::unique_ptr<MovementSystem>> m_movementSystems;
};
