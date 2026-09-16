#include "Map.h"

#include "Creature.h"
#include "Player.h"
#include "events/CharacterEvents.h"
#include "events/DropEvents.h"
#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"
#include "stats/Stats.h"
#include "tables/GameData.h"
#include "tables/MonsterTable.h"
#include "world/common/Paths.h"
#include "world/events/CreatureEvents.h"

#include <iostream>

Map::Map(MapRecord record, const GameData& data)
    : id(record.server_map_id), monster_file(std::move(record.monster_file)),
      npc_file(std::move(record.npc_file)), m_data(data)
{
    const MonsterTable& monsters = data.monsters;

    for (const auto& spawn : NpcScr::Load(Paths::Data.npc_spawn / (npc_file + ".scr")))
    {
        for (const auto& instance : spawn.instances)
        {
            Creature creature(CreatureKind::Npc, spawn.id, monsters);
            creature.placement.x = static_cast<std::uint32_t>(instance.x);
            creature.placement.y = static_cast<std::uint32_t>(instance.y);
            creature.placement.direction = static_cast<Direction>(instance.direction);
            Spawn(std::move(creature));
        }
    }

    for (const auto& group :
         MonsterSpawnScr::Load(Paths::Data.monster_spawn / (monster_file + ".scr")).groups)
    {
        for (const auto& instance : group.instances)
        {
            const std::uint32_t x = static_cast<std::uint32_t>(instance.x);
            const std::uint32_t y = static_cast<std::uint32_t>(instance.y);
            if (!IsInBounds(x, y))
            {
                std::cout << "Ignoring monster spawn " << group.monster_id
                          << " with out-of-range position (" << x << ", " << y << ")\n";
                continue;
            }

            Creature creature(CreatureKind::Monster, group.monster_id, monsters,
                              CreatureSpawn{
                                  .x = x,
                                  .y = y,
                                  .direction = static_cast<std::uint32_t>(instance.direction),
                              });
            creature.Respawn(kGridSize - 1);
            Spawn(std::move(creature));
        }
    }
}

bool Map::IsInBounds(std::uint32_t x, std::uint32_t y) noexcept
{
    return x < kGridSize && y < kGridSize;
}

void Map::Spawn(Creature creature)
{
    if (!IsInBounds(creature.placement.x, creature.placement.y))
    {
        std::cout << "Ignoring creature " << creature.monster_template.get().id
                  << " with out-of-range position (" << creature.placement.x << ", "
                  << creature.placement.y << ")\n";
        return;
    }

    creature.Bind(m_events);

    const std::uint32_t instanceId = creature.instance_id;
    if (!m_creatures.Add(instanceId, std::move(creature)))
        std::cout << "Ignoring duplicate creature instance " << instanceId << "\n";
}

bool Map::Spawn(Player player)
{
    const std::uint32_t instanceId = player.character.instance_id;
    const Zone::Coordinates zone =
        Zone::Of(player.character.placement.x, player.character.placement.y);

    // Whatever it had loaded belonged to its previous map.
    player.visible_players.clear();
    player.Bind(m_events);

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
    // Remove may swap another Player into this slot, invalidating `player`.
    const std::uint32_t instanceId = player.character.instance_id;
    std::optional<Player> removed = m_players.Remove(instanceId);
    if (removed)
    {
        removed->Unbind();
        m_events.Publish(CharacterLeaveEvent{.instance_id = instanceId});
    }

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
    const std::uint32_t fromX = player.character.placement.x;
    const std::uint32_t fromY = player.character.placement.y;

    player.character.placement.x = x;
    player.character.placement.y = y;
    player.character.placement.direction = static_cast<Direction>(direction);

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

Creature* Map::GetCreature(std::uint32_t instanceId)
{
    return m_creatures.Get(instanceId);
}

const Creature* Map::GetCreature(std::uint32_t instanceId) const
{
    return m_creatures.Get(instanceId);
}

std::vector<ConnectionId> Map::ViewersOf(Zone::Coordinates zone) const
{
    std::vector<ConnectionId> viewers;
    viewers.reserve(m_players.Size());

    for (const Player& player : m_players)
    {
        const Placement& placement = player.character.placement;
        if (Zone::IsNeighboring(Zone::Of(placement.x, placement.y), zone))
            viewers.push_back(player.connection);
    }

    return viewers;
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
    // Character bookkeeping before events, so anything reacting to this tick's events (e.g. a
    // same-tick join, a view change) sees fresh stats.derived; events before simulating, so they
    // see the state they were published against.
    TickCharacter(delta);
    m_events.Dispatch();

    // Runs regardless of players -- keeping the map clear of stale drops isn't for anyone's benefit.
    TickDrops(delta);
    TickMonster(delta);
}

void Map::TickCharacter(std::chrono::milliseconds delta)
{
    ForEachPlayer(
        [this](Player& player)
        {
            if (player.character.stats.dirty)
            {
                RecalculateDerivedStats(player.character, m_data);
                player.character.stats.dirty = false;
            }
        });
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

void Map::TickMonster(std::chrono::milliseconds delta)
{
    // Respawn is world state and keeps counting down on an empty map; wandering is only
    // simulated while somebody is there to see it.
    const bool simulateAi = HasPlayers();

    std::vector<std::uint32_t> readyToRespawn;

    for (Creature& creature : m_creatures)
    {
        if (creature.IsAlive() && !simulateAi)
            continue;

        creature.Tick(delta, kGridSize - 1);

        if (std::optional<CreatureMove> move = creature.TakePendingMove())
        {
            m_events.Publish(CreatureMoveEvent{
                .creature_id = move->creature_id,
                .from_x = move->from_x,
                .from_y = move->from_y,
                .to_x = move->to_x,
                .to_y = move->to_y,
                .movement_mode = move->movement_mode,
            });
        }

        if (creature.NeedsRespawn())
            readyToRespawn.push_back(creature.instance_id);
    }

    for (const std::uint32_t instanceId : readyToRespawn)
    {
        Creature* creature = m_creatures.Get(instanceId);
        if (!creature || !creature->spawn)
            continue;

        creature->Respawn(kGridSize - 1);
        m_events.Publish(CreatureRespawnEvent{.creature_id = instanceId});
    }
}
