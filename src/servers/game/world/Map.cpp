#include "Map.h"

#include "Creature.h"
#include "MapEvents.h"
#include "Player.h"
#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"
#include "world/common/EntityIdGenerator.h"
#include "world/common/Paths.h"

#include <algorithm>
#include <iostream>
#include <iterator>

namespace
{
bool InZoneGrid(std::int32_t zoneX, std::int32_t zoneY)
{
    return zoneX >= 0 && zoneX < Map::kZoneGridSize && zoneY >= 0 && zoneY < Map::kZoneGridSize;
}

std::size_t ZoneIndex(std::int32_t zoneX, std::int32_t zoneY)
{
    return static_cast<std::size_t>(zoneY) * static_cast<std::size_t>(Map::kZoneGridSize) +
           static_cast<std::size_t>(zoneX);
}

} // namespace

Map::Map(MapRecord record)
    : id(record.server_map_id), monster_file(std::move(record.monster_file)),
      npc_file(std::move(record.npc_file)), m_zones(CreateZones())
{
    for (const auto& spawn : NpcScr::Load(Paths::Data.npc_spawn / (npc_file + ".scr")))
    {
        for (const auto& instance : spawn.instances)
        {
            AddCreature(Creature{
                .instance_id = EntityIdGenerator::Next(),
                .kind = CreatureKind::Npc,
                .monster_id = spawn.id,
                .x = instance.x,
                .y = instance.y,
                .direction = instance.direction,
            });
        }
    }

    for (const auto& group :
         MonsterSpawnScr::Load(Paths::Data.monster_spawn / (monster_file + ".scr")).groups)
    {
        for (const auto& instance : group.instances)
        {
            AddCreature(Creature{
                .instance_id = EntityIdGenerator::Next(),
                .kind = CreatureKind::Monster,
                .monster_id = group.monster_id,
                .x = instance.x,
                .y = instance.y,
                .direction = instance.direction,
            });
        }
    }
}

void Map::AddItem(GroundItem item)
{
    std::lock_guard lock(m_itemsMutex);
    m_items.push_back(item);
}

bool Map::RemoveItem(std::uint32_t id)
{
    std::lock_guard lock(m_itemsMutex);
    auto it = std::find_if(m_items.begin(), m_items.end(),
                           [id](const GroundItem& item) { return item.id == id; });
    if (it == m_items.end())
        return false;

    m_items.erase(it);
    return true;
}

std::optional<GroundItem> Map::TryTakeItem(std::uint32_t id)
{
    std::lock_guard lock(m_itemsMutex);
    auto it = std::find_if(m_items.begin(), m_items.end(),
                           [id](const GroundItem& item) { return item.id == id; });
    if (it == m_items.end())
        return std::nullopt;

    GroundItem taken = *it;
    m_items.erase(it);
    return taken;
}

std::vector<GroundItem> Map::Items() const
{
    std::lock_guard lock(m_itemsMutex);
    return m_items;
}

void Map::AddCreature(Creature creature)
{
    const auto [zoneX, zoneY] = ZoneOf(creature.x, creature.y);
    if (!InZoneGrid(zoneX, zoneY))
    {
        std::cout << "Ignoring creature " << creature.monster_id << " with out-of-range position ("
                  << creature.x << ", " << creature.y << ")\n";
        return;
    }

    std::unique_lock lock(m_zonesMutex);
    m_zones[ZoneIndex(zoneX, zoneY)].AddCreature(std::move(creature));
}

std::vector<Creature> Map::CreaturesInZone(std::int32_t zoneX, std::int32_t zoneY) const
{
    if (!InZoneGrid(zoneX, zoneY))
        return {};

    std::shared_lock lock(m_zonesMutex);
    return m_zones[ZoneIndex(zoneX, zoneY)].CreatureSnapshot();
}

bool Map::Spawn(Player player)
{
    const std::uint32_t instanceId = player.character.instance_id;
    if (!m_players.Add(instanceId, std::move(player)))
        return false;

    m_events.Publish(CharacterJoinEvent{.instance_id = instanceId});
    return true;
}

std::optional<Player> Map::Despawn(std::uint32_t instanceId)
{
    return m_players.Remove(instanceId);
}

Player* Map::GetPlayer(std::uint32_t instanceId)
{
    return m_players.Get(instanceId);
}

const Player* Map::GetPlayer(std::uint32_t instanceId) const
{
    return m_players.Get(instanceId);
}

void Map::SetPlayer(SOCKET socket, std::int32_t x, std::int32_t y)
{
    std::lock_guard lock(m_mapPlayersMutex);
    m_mapPlayers[socket] = MapPlayer{.socket = socket, .x = x, .y = y};
}

void Map::RemovePlayer(SOCKET socket)
{
    std::lock_guard lock(m_mapPlayersMutex);
    m_mapPlayers.erase(socket);
}

std::vector<Map::MapPlayer> Map::Players() const
{
    std::vector<MapPlayer> players;

    std::lock_guard lock(m_mapPlayersMutex);
    players.reserve(m_mapPlayers.size());
    for (const auto& [socket, player] : m_mapPlayers)
        players.push_back(player);

    return players;
}

bool Map::HasPlayers() const
{
    std::lock_guard lock(m_mapPlayersMutex);
    return !m_mapPlayers.empty();
}

std::pair<std::int32_t, std::int32_t> Map::ZoneOf(std::int32_t x, std::int32_t y)
{
    return Zone::Of(x, y);
}

EventBus& Map::Events()
{
    return m_events;
}

void Map::Tick(std::chrono::milliseconds delta)
{
    // Events first, so they see the state they were published against; then simulate.
    m_events.Dispatch();

    // Nobody to see creatures on an empty map, but its events still went out above.
    if (HasPlayers())
        TickCreature(delta);
}

std::vector<Map::CreatureMove> Map::TickCreature(std::chrono::milliseconds delta)
{
    std::vector<Creature> relocated;
    std::vector<CreatureMove> moves;

    std::unique_lock lock(m_zonesMutex);
    for (Zone& zone : m_zones)
    {
        auto result = zone.Tick(delta, kGridSize - 1);
        moves.insert(moves.end(), std::make_move_iterator(result.moves.begin()),
                     std::make_move_iterator(result.moves.end()));
        relocated.insert(relocated.end(), std::make_move_iterator(result.relocated.begin()),
                         std::make_move_iterator(result.relocated.end()));
    }

    for (Creature& creature : relocated)
    {
        const auto [zoneX, zoneY] = ZoneOf(creature.x, creature.y);
        m_zones[ZoneIndex(zoneX, zoneY)].AddCreature(std::move(creature));
    }

    return moves;
}

std::vector<std::pair<std::int32_t, std::int32_t>> Map::ZonesAround(std::int32_t x,
                                                                    std::int32_t y) const
{
    std::vector<std::pair<std::int32_t, std::int32_t>> zones;
    const auto [zoneX, zoneY] = ZoneOf(x, y);

    for (std::int32_t dy = -1; dy <= 1; ++dy)
    {
        for (std::int32_t dx = -1; dx <= 1; ++dx)
        {
            const std::int32_t neighborX = zoneX + dx;
            const std::int32_t neighborY = zoneY + dy;
            if (InZoneGrid(neighborX, neighborY))
                zones.emplace_back(neighborX, neighborY);
        }
    }

    return zones;
}

std::vector<Zone> Map::CreateZones()
{
    std::vector<Zone> zones;
    zones.reserve(kZoneCount);

    for (std::int32_t zoneY = 0; zoneY < kZoneGridSize; ++zoneY)
    {
        for (std::int32_t zoneX = 0; zoneX < kZoneGridSize; ++zoneX)
            zones.emplace_back(zoneX, zoneY);
    }

    return zones;
}
