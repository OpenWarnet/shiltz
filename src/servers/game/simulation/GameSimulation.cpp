#include "GameSimulation.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "parser/MapScr.h"
#include "protocol/server/CharMoveUpdate.h"
#include "protocol/server/CrtLoad.h"
#include "protocol/server/CrtMove.h"
#include "protocol/server/ItemMapNew.h"
#include "protocol/server/ViewRemoveAll.h"
#include "simulation/CreatureModule.h"
#include "simulation/GroundItemModule.h"
#include "simulation/PlayerMoveModule.h"
#include "tables/GameData.h"
#include "tables/MonsterTable.h"

#include <exception>
#include <iostream>

namespace
{
    // The coordinate space every map shares. The v1 Map hardcoded the same
    // 512, and map.scr carries no per-map extent, so one constant is the
    // honest answer rather than a lookup that would always agree.
    constexpr int kMapExtent = 512;

    // No collision data exists anywhere in the server: map.scr has none,
    // and neither do the npc/monster spawn files. So every tile in bounds
    // is walkable, and Tile::Move refuses a step only at the map edge.
    //
    // When terrain does land it loads through Tile::SetWalkable and both
    // PlayerMoveSystem and CreatureWanderSystem start enforcing it without
    // changing -- Tile::Move already checks it.
    constexpr bool kAllTilesWalkable = true;

    std::filesystem::path DataDir()
    {
        return std::filesystem::path(SHILTZ_SOURCE_DIR) / "src" / "servers" / "game" / "world" / "data";
    }

    std::filesystem::path MonsterSpawnDataDir()
    {
        return DataDir() / "spawn" / "monster";
    }

    std::filesystem::path NpcSpawnDataDir()
    {
        return DataDir() / "spawn" / "npc";
    }

    world_v2::ConnectionId ToConnection(SOCKET socket)
    {
        return static_cast<world_v2::ConnectionId>(socket);
    }

    SOCKET ToSocket(world_v2::ConnectionId connection)
    {
        return static_cast<SOCKET>(connection);
    }
} // namespace

GameSimulation::GameSimulation(Server& server, std::span<const std::uint8_t> key, GameSessionStore& sessions,
                               const GameData& data)
    : m_server(server)
    , m_key(key)
    , m_sessions(sessions)
    , m_data(data)
{
}

void GameSimulation::Start()
{
    // Only the file names, not the maps themselves. Building all 581 would
    // cost gigabytes of Tile grid to simulate the handful anyone stands on.
    for (const auto& record : MapScr::Load(DataDir() / "map.scr"))
    {
        // -1 (and any other non-positive value) marks an unused map.scr
        // slot -- every other field on those rows is blank too.
        if (record.server_map_id <= 0)
        {
            continue;
        }

        m_mapFiles.emplace(static_cast<std::uint32_t>(record.server_map_id),
                           MapFiles{NpcSpawnDataDir() / (record.npc_file + ".scr"),
                                    MonsterSpawnDataDir() / (record.monster_file + ".scr")});
    }

    std::cout << "World started -- " << m_mapFiles.size() << " maps known\n";
}

void GameSimulation::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maps.clear();
}

void GameSimulation::OnPickup(PickupHandler handler)
{
    m_onPickup = std::move(handler);
}

std::uint32_t GameSimulation::AllocateNetworkId()
{
    return m_nextNetworkId.fetch_add(1, std::memory_order_relaxed);
}

world_v2::Simulation* GameSimulation::SimulationForMap(std::uint32_t mapId)
{
    const auto existing = m_maps.find(mapId);
    if (existing != m_maps.end())
    {
        return existing->second.get();
    }

    const auto files = m_mapFiles.find(mapId);
    if (files == m_mapFiles.end())
    {
        return nullptr;
    }

    auto simulation = std::make_unique<world_v2::Simulation>(kMapExtent, kMapExtent, kAllTilesWalkable,
                                                             static_cast<world_v2::SimulationId>(mapId));

    // Some map.scr rows name a spawn file that is not present under
    // data/spawn/{npc,monster}. Carrying on without creatures is what the
    // v1 loader did too -- one data gap should not take a map down, and
    // certainly should not take down the player trying to enter it.
    try
    {
        simulation->Install<game_sim::CreatureModule>(files->second.npcScr, files->second.monsterScr,
                                                      [this] { return AllocateNetworkId(); });
    }
    catch (const std::exception& error)
    {
        std::cout << "map " << mapId << ": no creatures (" << error.what() << ")\n";
    }

    simulation->Install<game_sim::PlayerMoveModule>(
        [this](world_v2::ConnectionId connection, const CharMoveUpdate& update) {
            SendMoveUpdate(connection, update);
        });

    simulation->Install<game_sim::GroundItemModule>(
        [this](const game_sim::ItemClaimedEvent& event)
        {
            if (m_onPickup)
            {
                m_onPickup(ToSocket(event.connection), event.itemNetworkId, event.slotId, event.item);
            }
        });

    // Last, so stage 4 describes a world whose creatures and players have
    // already moved this tick.
    simulation->Install<game_sim::ViewModule>(
        [this](world_v2::Map& world, const game_sim::ViewModule::ViewDelta& delta) { PublishView(world, delta); });

    world_v2::Simulation* raw = simulation.get();
    m_maps.emplace(mapId, std::move(simulation));
    return raw;
}

bool GameSimulation::Join(SOCKET socket, std::uint32_t mapId, int x, int y, std::uint32_t instanceId, float speed)
{
    if (mapId == 0 || mapId > 0xFFFFu)
    {
        // SimulationId is 16-bit and 0 is reserved -- see world_v2/Ids.h.
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    world_v2::Simulation* simulation = SimulationForMap(mapId);
    if (simulation == nullptr)
    {
        return false;
    }

    // Structural, so it goes through the queue like everything else and
    // lands at the top of the next tick rather than here.
    simulation->Commands().Push(
        world_v2::JoinCommand{ToConnection(socket), static_cast<world_v2::CharacterId>(instanceId), x, y});

    // The speed has to reach an entity the join has not created yet. Pushed
    // straight after, so it drains in the same batch once the entity
    // exists -- commands apply in push order across all types.
    simulation->Commands().Push(game_sim::PlayerSpeedCommand{ToConnection(socket), speed});

    m_mapBySocket[socket] = mapId;
    return true;
}

void GameSimulation::Leave(SOCKET socket)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto found = m_mapBySocket.find(socket);
    if (found == m_mapBySocket.end())
    {
        return;
    }

    const auto map = m_maps.find(found->second);
    if (map != m_maps.end())
    {
        map->second->Commands().Push(world_v2::LeaveCommand{ToConnection(socket)});
    }

    m_mapBySocket.erase(found);
}

void GameSimulation::PushMove(SOCKET socket, int targetX, int targetY, std::uint32_t direction,
                              std::uint32_t stopDirection)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto found = m_mapBySocket.find(socket);
    if (found == m_mapBySocket.end())
    {
        // A move from a connection that never entered a map. Normal enough
        // during a disconnect race -- dropped, not an error.
        return;
    }

    const auto map = m_maps.find(found->second);
    if (map == m_maps.end())
    {
        return;
    }

    map->second->Commands().Push(
        game_sim::PlayerMoveCommand{ToConnection(socket), targetX, targetY, direction, stopDirection});
}

void GameSimulation::PushPickup(SOCKET socket, std::uint32_t itemNetworkId, std::uint32_t slotId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto found = m_mapBySocket.find(socket);
    if (found == m_mapBySocket.end())
    {
        return;
    }

    const auto map = m_maps.find(found->second);
    if (map == m_maps.end())
    {
        return;
    }

    map->second->Commands().Push(game_sim::PickupCommand{ToConnection(socket), itemNetworkId, slotId});
}

std::uint32_t GameSimulation::DropItem(std::uint32_t mapId, int x, int y, const Item& item)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto map = m_maps.find(mapId);
    if (map == m_maps.end())
    {
        return 0;
    }

    // Allocated here rather than in the tick, because the dropping client
    // is told the id in its own acknowledgement, which goes out before the
    // tick that creates the entity has run.
    const std::uint32_t networkId = AllocateNetworkId();
    map->second->Commands().Push(game_sim::DropItemCommand{world_v2::kInvalidConnection, networkId, x, y, item});

    return networkId;
}

void GameSimulation::Tick(float deltaSeconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [mapId, simulation] : m_maps)
    {
        simulation->Tick(deltaSeconds);
    }
}

void GameSimulation::Send(SOCKET socket, GameOpcode::Code opcode, const std::vector<std::uint8_t>& payload)
{
    GamePacket packet(opcode, payload);
    m_server.SendTo(socket, packet.Serialize(m_key));
}

void GameSimulation::SendMoveUpdate(world_v2::ConnectionId connection, const CharMoveUpdate& update)
{
    const SOCKET socket = ToSocket(connection);

    // The simulation is authoritative, so the session's copy of the
    // position is updated from the tick's answer rather than from the
    // packet that asked. Everything else on the server -- saving, the
    // bank's distance gate -- reads that copy.
    if (auto session = m_sessions.Get(socket))
    {
        session->player.x = static_cast<std::int32_t>(update.x);
        session->player.y = static_cast<std::int32_t>(update.y);
        session->player.direction = static_cast<std::int32_t>(update.direction);
        m_sessions.Set(socket, *session);
    }

    PayloadWriter writer;
    update.Serialize(writer);
    Send(socket, GameOpcode::GC_CHAR_MOVE, writer.Data());
}

// One viewer's diff, turned into the packets that client is owed.
//
// This is the whole outbound translation layer, and deliberately the only
// place in the server that knows both an Entity and an opcode. Everything
// above it reasons about entities; everything below it is bytes.
//
// The batching falls out of the diff for free: one tick per viewer yields
// one CrtLoad holding every creature that came into view and one
// ViewRemoveAll holding everything that left. That is the shape these
// packets already have -- CrtLoad carries a vector of records,
// ViewRemoveAll three vectors of ids -- and it is why a per-event stream
// was the wrong granularity for them.
void GameSimulation::PublishView(world_v2::Map& world, const game_sim::ViewModule::ViewDelta& delta)
{
    const SOCKET socket = ToSocket(delta.connection);

    if (!delta.enteredCreatures.empty())
    {
        CrtLoad load;
        for (const world_v2::Entity entity : delta.enteredCreatures)
        {
            const auto* creature = world.registry.TryGet<game_sim::CreatureComponent>(entity);
            const auto* network = world.registry.TryGet<game_sim::NetworkIdComponent>(entity);
            const auto* position = world.registry.TryGet<world_v2::GridPositionComponent>(entity);
            if (creature == nullptr || network == nullptr || position == nullptr)
            {
                continue;
            }

            const MonsterRecord* record = m_data.monsters.Find(creature->templateId);

            load.records.push_back(CrtLoadRecord{
                .id = network->id,
                .x = static_cast<std::uint32_t>(position->x),
                .y = static_cast<std::uint32_t>(position->y),
                .monster_id = static_cast<std::uint32_t>(creature->templateId),
                .direction = static_cast<std::uint32_t>(creature->direction),
                .hp = record ? static_cast<std::uint64_t>(record->hp) : 0,
            });
        }

        if (!load.records.empty())
        {
            PayloadWriter writer;
            load.Serialize(writer);
            Send(socket, GameOpcode::GC_CRT_LOAD, writer.Data());
        }
    }

    for (const world_v2::Entity entity : delta.enteredItems)
    {
        const auto* mapItem = world.registry.TryGet<game_sim::MapItemComponent>(entity);
        const auto* network = world.registry.TryGet<game_sim::NetworkIdComponent>(entity);
        const auto* position = world.registry.TryGet<world_v2::GridPositionComponent>(entity);
        if (mapItem == nullptr || network == nullptr || position == nullptr)
        {
            continue;
        }

        ItemMapNew appeared{
            .id = network->id,
            .x = static_cast<std::uint32_t>(position->x),
            .y = static_cast<std::uint32_t>(position->y),
            .item_id = static_cast<std::uint32_t>(mapItem->item.item_id),
            .owner_id = 0,
        };

        PayloadWriter writer;
        appeared.Serialize(writer);
        Send(socket, GameOpcode::GC_ITEM_MAP_NEW, writer.Data());
    }

    for (const auto& moved : delta.moved)
    {
        CrtMove move{
            .creature_id = moved.networkId,
            .x = static_cast<std::uint32_t>(moved.fromX),
            .y = static_cast<std::uint32_t>(moved.fromY),
            .target_x = static_cast<std::uint32_t>(moved.toX),
            .target_y = static_cast<std::uint32_t>(moved.toY),
            .speed_raw = 0,
        };

        PayloadWriter writer;
        move.Serialize(writer);
        Send(socket, GameOpcode::GC_CRT_MOVE, writer.Data());
    }

    if (!delta.leftCreatureIds.empty() || !delta.leftItemIds.empty() || !delta.leftPlayerIds.empty())
    {
        ViewRemoveAll removed;
        removed.creature_ids = delta.leftCreatureIds;
        removed.item_ids = delta.leftItemIds;
        removed.player_ids = delta.leftPlayerIds;

        PayloadWriter writer;
        removed.Serialize(writer);
        Send(socket, GameOpcode::GC_VIEW_REMOVE_ALL, writer.Data());
    }
}
