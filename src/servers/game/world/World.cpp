#include "World.h"

#include "world/Player.h"
#include "world/common/Paths.h"
#include "parser/MapScr.h"
#include "tables/GameData.h"

#include <exception>
#include <iostream>
#include <utility>

World::World() = default;
World::~World() = default;

void World::Start(const Outbox& outbox, const GameData& data)
{
    std::cout << "World started\n";

    for (auto& record : MapScr::Load(Paths::Data.root / "map.scr"))
    {
        try
        {
            m_atlas.Add(std::move(record), data.monsters);
        }
        catch (const std::exception& e)
        {
            std::cout << "Skipping map: " << e.what() << "\n";
        }
    }

    m_atlas.ForEach(
        [&](Map& map)
        {
            for (System::Factory factory : System::Factories())
                m_systems.push_back(factory(map, outbox, data));
        });
}

void World::Shutdown()
{
    std::cout << "World shut down\n";
}

Map* World::GetMap(std::int64_t id)
{
    return m_atlas.Get(id);
}

const Map* World::GetMap(std::int64_t id) const
{
    return m_atlas.Get(id);
}

void World::Connect(ConnectionId connection)
{
    m_connections.insert(connection);
}

void World::Disconnect(ConnectionId connection)
{
    m_connections.erase(connection);
}

bool World::IsConnected(ConnectionId connection) const
{
    return m_connections.contains(connection);
}

bool World::Join(Player player)
{
    Map* map = m_atlas.Get(player.character.map_id);
    if (!map || IsOnline(player.character.id) || m_playersByConnection.contains(player.connection))
        return false;

    const ConnectionId connection = player.connection;
    const PlayerRef ref{
        .mapId = map->id,
        .instanceId = player.character.instance_id,
        .characterId = player.character.id,
    };
    if (!map->Spawn(std::move(player)))
        return false;

    m_playersByConnection.emplace(connection, ref);
    m_connectionsByCharacter.emplace(ref.characterId, connection);
    return true;
}

std::optional<Player> World::Leave(ConnectionId connection)
{
    auto it = m_playersByConnection.find(connection);
    if (it == m_playersByConnection.end())
        return std::nullopt;

    const PlayerRef ref = it->second;
    m_playersByConnection.erase(it);
    m_connectionsByCharacter.erase(ref.characterId);

    Map* map = m_atlas.Get(ref.mapId);
    return map ? map->Despawn(ref.instanceId) : std::nullopt;
}

Player* World::FindPlayer(ConnectionId connection)
{
    auto it = m_playersByConnection.find(connection);
    if (it == m_playersByConnection.end())
        return nullptr;

    Map* map = m_atlas.Get(it->second.mapId);
    return map ? map->GetPlayer(it->second.instanceId) : nullptr;
}

bool World::IsOnline(std::int64_t characterId) const
{
    return m_connectionsByCharacter.contains(characterId);
}

void World::Receive(Request request)
{
    ingress.Push(std::move(request));
}

void World::Tick(std::chrono::milliseconds delta)
{
    ingress.DrainTo(m_requests);
    for (const auto& request : m_requests)
        request.message->Handle(request.context);

    m_atlas.Tick(delta);
}
