#include "Map.h"

#include "Creature.h"
#include "MapEvents.h"
#include "Player.h"
#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"
#include "tables/MonsterTable.h"
#include "world/common/EntityIdGenerator.h"
#include "world/common/Paths.h"

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
    const Zone::Coordinates zone = Zone::Of(player.character.x, player.character.y);

    // Whatever it had loaded belonged to its previous map.
    player.visible_players.clear();

    if (!m_players.Add(instanceId, std::move(player)))
        return false;

    m_events.Publish(CharacterJoinEvent{.instance_id = instanceId});

    // A placement has no previous view, so MovementSystem loads everything around it.
    m_events.Publish(CharacterZoneChangeEvent{.instance_id = instanceId, .to = zone});
    return true;
}

void Map::Spawn(Drop drop)
{
    const std::uint32_t id = drop.id, x = drop.x, y = drop.y, itemId = drop.item.item_id;
    if (!m_drops.Add(id, std::move(drop)))
        return; // id collision is practically impossible (EntityIdGenerator), guard kept for consistency with Spawn(Player)

    m_events.Publish(DropAddEvent{.id = id, .x = x, .y = y, .item_id = itemId});
}

std::optional<Player> Map::Despawn(const Player& player)
{
    std::optional<Player> removed = m_players.Remove(player.character.instance_id);
    if (removed)
        m_events.Publish(CharacterLeaveEvent{.instance_id = player.character.instance_id});

    return removed;
}

std::optional<Drop> Map::Despawn(const Drop& drop)
{
    std::optional<Drop> removed = m_drops.Remove(drop.id);
    if (removed)
        m_events.Publish(DropRemoveEvent{.id = removed->id, .x = removed->x, .y = removed->y});

    return removed;
}

bool Map::Move(Player& player, std::uint32_t x, std::uint32_t y, std::uint32_t direction,
               std::uint32_t speed, std::uint32_t stopDirection)
{
    if (!IsInBounds(x, y))
        return false;

    const std::uint32_t instanceId = player.character.instance_id;
    const std::uint32_t fromX = player.character.x;
    const std::uint32_t fromY = player.character.y;

    player.character.x = x;
    player.character.y = y;
    player.character.direction = direction;

    // Published before the move so the view updates go out ahead of GC_CHAR_MOVE.
    if (Zone::Crossed(fromX, fromY, x, y))
    {
        m_events.Publish(CharacterZoneChangeEvent{
            .instance_id = instanceId,
            .from = Zone::Of(fromX, fromY),
            .to = Zone::Of(x, y),
        });
    }

    m_events.Publish(CharacterMoveEvent{
        .instance_id = instanceId,
        .from_x = fromX,
        .from_y = fromY,
        .to_x = x,
        .to_y = y,
        .direction = direction,
        .speed = speed,
        .stop_direction = stopDirection,
    });

    return true;
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

    // Runs regardless of players -- keeping the map clear of stale drops isn't for anyone's benefit.
    TickDrops(delta);

    // Nobody to see creatures on an empty map, but its events still went out above.
    if (HasPlayers())
        TickCreature(delta);
}

void Map::TickDrops(std::chrono::milliseconds delta)
{
    std::vector<Drop> expired;
    ForEachDrop(
        [&](Drop& drop)
        {
            drop.time_to_live -= delta;
            if (drop.time_to_live <= std::chrono::milliseconds::zero())
                expired.push_back(drop);
        });

    for (const Drop& drop : expired)
        Despawn(drop);
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
