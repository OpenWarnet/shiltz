#include "World.h"

#include "world/Player.h"
#include "world/common/Paths.h"
#include "world/systems/EnterSystem.h"
#include "world/systems/MovementSystem.h"
#include "parser/MapScr.h"

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
            m_atlas.Add(std::move(record));
        }
        catch (const std::exception& e)
        {
            std::cout << "Skipping map: " << e.what() << "\n";
        }
    }

    m_atlas.ForEach(
        [&](Map& map)
        {
            m_enterSystems.push_back(std::make_unique<EnterSystem>(map, outbox, data));
            m_movementSystems.push_back(std::make_unique<MovementSystem>(map, outbox, data));
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

bool World::Join(Player player)
{
    Map* map = m_atlas.Get(player.character.map_id);
    if (!map || IsOnline(player.character.id) || m_playersByConnection.contains(player.socket))
        return false;

    const SOCKET connection = player.socket;
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

std::optional<Player> World::Leave(SOCKET connection)
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

Player* World::FindPlayer(SOCKET connection)
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
