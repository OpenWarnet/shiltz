#pragma once

#include "GameOpcodes.h"
#include "simulation/Components.h"
#include "simulation/ViewModule.h"
#include "world/Item.h"
#include "world_v2/Simulation.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>
#include <winsock2.h>

class Server;
class GameSessionStore;
class GameData;
struct CharMoveUpdate;

// The game server's live world, and the only place that knows both a socket
// and an Entity.
//
// One world_v2::Simulation per map, each holding that map's creatures,
// ground items and players. This is the whole simulation -- the v1
// World/Map pair it replaced is gone, along with its zone-bucketed creature
// grid, its per-map mutexes and its own tick pool.
//
// What a map is made of
// ---------------------
//     CreatureModule    npcNN.scr + mNN.scr -> entities, and the wander AI
//     PlayerMoveModule  CG_MOVE -> authoritative position -> GC_CHAR_MOVE
//     GroundItemModule  drops, and pickup claimed at the barrier
//     ViewModule        per-viewer visible-set diff -> the view packets
//
// Installed in that order, so stage 2 wanders creatures and resolves player
// moves before stage 4 describes the settled result.
//
// Maps are built on demand. map.scr has 581 rows and a 512x512 Tile grid
// costs about 7.6 MB, so building every map at startup would cost gigabytes
// to simulate the handful anyone is standing on. A map is built the first
// time somebody enters it -- loading that map's creature files at that
// point -- and kept afterwards.
//
// Threading
// ---------
// Join, Leave, PushMove, PushPickup and DropItem are called from
// per-connection handler threads and only ever push to a CommandQueue,
// which is its own synchronized surface. Tick is called from GameServer's
// world strand, which serializes it against itself. m_mutex covers the map
// table and the connection index, nothing deeper -- no handler thread ever
// touches a Registry.
class GameSimulation
{
public:
    GameSimulation(Server& server, std::span<const std::uint8_t> key, GameSessionStore& sessions,
                   const GameData& data);

    GameSimulation(const GameSimulation&) = delete;
    GameSimulation& operator=(const GameSimulation&) = delete;

    // Reads map.scr to learn which maps exist and which creature files each
    // one uses. No map is built yet -- that happens on first entry.
    void Start();

    void Shutdown();

    // Puts a character into its map at its saved position, building the map
    // if this is the first arrival. False if the map id is unknown to
    // map.scr or the saved position is off the map.
    bool Join(SOCKET socket, std::uint32_t mapId, int x, int y, std::uint32_t instanceId, float speed);

    // Takes a connection out of whichever map it was on. Safe for a socket
    // that never joined.
    void Leave(SOCKET socket);

    // Queues a CG_MOVE. Resolved on the next tick, not now -- the answer
    // depends on where the simulation says the player is, and that is only
    // knowable once the tick runs.
    void PushMove(SOCKET socket, int targetX, int targetY, std::uint32_t direction, std::uint32_t stopDirection);

    // Queues a CG_ITEM_PICKUP. The claim happens at the barrier, so two
    // players racing the same item cannot both win it.
    void PushPickup(SOCKET socket, std::uint32_t itemNetworkId, std::uint32_t slotId);

    // Puts an item on the ground and returns the network id the client will
    // know it by, or 0 if that map is not loaded. Queued like everything
    // else; the item exists from the next tick.
    std::uint32_t DropItem(std::uint32_t mapId, int x, int y, const Item& item);

    // Advances every live map. World strand only.
    void Tick(float deltaSeconds);

    // Called on the world strand when a pickup claim succeeds. The item is
    // already off the map by then, so the handler's job is the database
    // write and the acknowledgement -- and it must not block, which means
    // posting the database half elsewhere.
    using PickupHandler =
        std::function<void(SOCKET socket, std::uint32_t itemNetworkId, std::uint32_t slotId, const Item& item)>;
    void OnPickup(PickupHandler handler);

private:
    struct MapFiles
    {
        std::filesystem::path npcScr;
        std::filesystem::path monsterScr;
    };

    world_v2::Simulation* SimulationForMap(std::uint32_t mapId);

    void SendMoveUpdate(world_v2::ConnectionId connection, const CharMoveUpdate& update);
    void PublishView(world_v2::Map& world, const game_sim::ViewModule::ViewDelta& delta);
    void Send(SOCKET socket, GameOpcode::Code opcode, const std::vector<std::uint8_t>& payload);

    std::uint32_t AllocateNetworkId();

    Server& m_server;
    std::span<const std::uint8_t> m_key;
    GameSessionStore& m_sessions;
    const GameData& m_data;

    // Unique across every map, the way the v1 World's creature instance ids
    // were. A client only sees one map at a time, but a warp must not hand
    // it an id it is still holding for something else. Starts clear of
    // anything the .scr data uses.
    std::atomic<std::uint32_t> m_nextNetworkId{10000};

    mutable std::mutex m_mutex;
    std::unordered_map<std::uint32_t, MapFiles> m_mapFiles;
    std::unordered_map<std::uint32_t, std::unique_ptr<world_v2::Simulation>> m_maps;
    std::unordered_map<SOCKET, std::uint32_t> m_mapBySocket;

    PickupHandler m_onPickup;
};
