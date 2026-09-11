#include "Map.h"

#include "Creature.h"
#include "MapEvents.h"
#include "Player.h"
#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"
#include "tables/MonsterTable.h"
#include "world/common/EntityIdGenerator.h"
#include "world/common/Paths.h"

#include <algorithm>
#include <iostream>
#include <iterator>

namespace
{
std::int64_t MaxHp(const MonsterTable& monsters, std::int64_t monsterId)
{
    const MonsterRecord* record = monsters.Find(monsterId);
    return record ? record->hp : 0;
}

} // namespace

Map::Map(MapRecord record, const MonsterTable& monsters)
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
                .monster_id = static_cast<std::uint64_t>(spawn.id),
                .x = static_cast<std::uint32_t>(instance.x),
                .y = static_cast<std::uint32_t>(instance.y),
                .direction = static_cast<std::uint32_t>(instance.direction),
                .hp = MaxHp(monsters, spawn.id),
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
                .monster_id = static_cast<std::uint64_t>(group.monster_id),
                .x = static_cast<std::uint32_t>(instance.x),
                .y = static_cast<std::uint32_t>(instance.y),
                .direction = static_cast<std::uint32_t>(instance.direction),
                .hp = MaxHp(monsters, group.monster_id),
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

bool Map::IsInBounds(std::uint32_t x, std::uint32_t y) noexcept
{
    return x < kGridSize && y < kGridSize;
}

void Map::AddCreature(Creature creature)
{
    if (!IsInBounds(creature.x, creature.y))
    {
        std::cout << "Ignoring creature " << creature.monster_id << " with out-of-range position ("
                  << creature.x << ", " << creature.y << ")\n";
        return;
    }

    m_zones[Zone::Of(creature.x, creature.y).Index()].AddCreature(std::move(creature));
}

std::span<const Creature> Map::CreaturesInZone(Zone::Coordinates zone) const
{
    if (!Zone::IsInGrid(zone))
        return {};

    return m_zones[zone.Index()].Creatures();
}

bool Map::Spawn(Player player)
{
    const std::uint32_t instanceId = player.character.instance_id;

    // Whatever it had loaded belonged to its previous map.
    player.visible_players.clear();

    if (!m_players.Add(instanceId, std::move(player)))
        return false;

    m_events.Publish(CharacterJoinEvent{.instance_id = instanceId});
    return true;
}

std::optional<Player> Map::Despawn(std::uint32_t instanceId)
{
    std::optional<Player> player = m_players.Remove(instanceId);
    if (player)
        m_events.Publish(CharacterLeaveEvent{.instance_id = instanceId});

    return player;
}

Player* Map::GetPlayer(std::uint32_t instanceId)
{
    return m_players.Get(instanceId);
}

const Player* Map::GetPlayer(std::uint32_t instanceId) const
{
    return m_players.Get(instanceId);
}

bool Map::HasPlayers() const
{
    return !m_players.Empty();
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

std::vector<Zone::CreatureMove> Map::TickCreature(std::chrono::milliseconds delta)
{
    std::vector<Creature> relocated;
    std::vector<Zone::CreatureMove> moves;

    for (Zone& zone : m_zones)
    {
        auto result = zone.Tick(delta, kGridSize - 1);
        moves.insert(moves.end(), std::make_move_iterator(result.moves.begin()),
                     std::make_move_iterator(result.moves.end()));
        relocated.insert(relocated.end(), std::make_move_iterator(result.relocated.begin()),
                         std::make_move_iterator(result.relocated.end()));
    }

    for (Creature& creature : relocated)
        m_zones[Zone::Of(creature.x, creature.y).Index()].AddCreature(std::move(creature));

    return moves;
}

std::vector<Zone> Map::CreateZones()
{
    std::vector<Zone> zones;
    zones.reserve(kZoneCount);

    constexpr auto kLimit = static_cast<std::int32_t>(kZoneGridSize);
    for (std::int32_t zoneY = 0; zoneY < kLimit; ++zoneY)
    {
        for (std::int32_t zoneX = 0; zoneX < kLimit; ++zoneX)
            zones.emplace_back(zoneX, zoneY);
    }

    return zones;
}
