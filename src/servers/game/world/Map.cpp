#include "Map.h"

#include "Creature.h"
#include "Grid.h"
#include "Player.h"
#include "events/CharacterEvents.h"
#include "events/DropEvents.h"
#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"
#include "stats/Stats.h"
#include "tables/GameData.h"
#include "tables/MonsterTable.h"
#include "world/common/Paths.h"
#include "world/ai/CreatureAiSensing.h"
#include "world/events/CreatureEvents.h"

#include <algorithm>
#include <iostream>
#include <type_traits>
#include <vector>

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
            if (!Grid::Contains(x, y))
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
            creature.Respawn();
            Spawn(std::move(creature));
        }
    }
}

void Map::Spawn(Creature creature)
{
    if (!Grid::Contains(creature.placement.x, creature.placement.y))
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

    m_visibility.Add(instanceId, zone);
    m_events.Publish(CharacterJoinEvent{.instance_id = instanceId});

    // A placement has no previous view, so MovementSystem loads everything around it.
    m_events.Publish(CharacterZoneChangeEvent{.instance_id = instanceId, .to = zone});
    return true;
}

void Map::Spawn(Drop drop)
{
    const std::uint32_t id = drop.id, x = drop.x, y = drop.y, itemId = drop.item.item_id;
    if (!m_drops.Add(id, std::move(drop)))
        return; // id collision is practically impossible (EntityIdGenerator), guard kept for
                // consistency with Spawn(Player)

    m_events.Publish(DropAddEvent{.id = id, .x = x, .y = y, .item_id = itemId});
}

std::optional<Player> Map::Despawn(const Player& player)
{
    // Remove may swap another Player into this slot, invalidating `player`.
    const std::uint32_t instanceId = player.character.instance_id;
    const Zone::Coordinates zone =
        Zone::Of(player.character.placement.x, player.character.placement.y);
    std::optional<Player> removed = m_players.Remove(instanceId);
    if (removed)
    {
        m_visibility.Remove(instanceId, zone);
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
    if (!Grid::Contains(x, y))
        return false;

    const std::uint32_t instanceId = player.character.instance_id;
    const std::uint32_t fromX = player.character.placement.x;
    const std::uint32_t fromY = player.character.placement.y;
    const Zone::Coordinates fromZone = Zone::Of(fromX, fromY);
    const Zone::Coordinates toZone = Zone::Of(x, y);

    player.character.placement.x = x;
    player.character.placement.y = y;
    player.character.placement.direction = static_cast<Direction>(direction);

    // Published before the move so the view updates go out ahead of GC_CHAR_MOVE.
    if (fromZone != toZone)
    {
        m_visibility.Move(instanceId, fromZone, toZone);
        m_events.Publish(CharacterZoneChangeEvent{
            .instance_id = instanceId,
            .from = fromZone,
            .to = toZone,
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

    // Runs regardless of players -- keeping the map clear of stale drops isn't for anyone's
    // benefit.
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

    for (Creature& creature : m_creatures)
    {
        if (!creature.IsAlive())
        {
            creature.Tick(delta);
            continue;
        }

        if (!simulateAi || creature.kind != CreatureKind::Monster)
            continue;

        TickCreatureAi(creature, delta);
    }
}

void Map::TickCreatureAi(Creature& creature, std::chrono::milliseconds delta)
{
    const std::optional<CreatureAiSenseRequest> request = creature.m_ai.Advance(delta);
    if (!request)
        return;

    const CreatureAiPerception perception = BuildCreaturePerception(creature, *request);
    const std::optional<CreatureAiIntent> intent =
        creature.m_ai.Decide(creature.placement, creature.monster_template.get(), perception);
    if (intent)
        CommitCreatureAiIntent(creature, *intent);
}

CreatureAiPerception Map::BuildCreaturePerception(const Creature& creature,
                                                  const CreatureAiSenseRequest& request) const
{
    CreatureAiPerception perception;

    if (request.kind == CreatureAiSenseKind::AggroCandidate)
    {
        perception.aggro_candidate = creature_ai::SelectAggroCandidate(
            creature.placement, creature.monster_template.get(), m_players);
    }
    else if (request.kind == CreatureAiSenseKind::CurrentTarget)
    {
        const Player* target = GetPlayer(request.target_id);
        if (target && target->character.hp > 0)
        {
            perception.current_target = CreatureAiObservation{
                target->character.instance_id, target->character.placement};
        }
    }

    return perception;
}

void Map::CommitCreatureAiIntent(Creature& creature, const CreatureAiIntent& intent)
{
    std::visit(
        [&](const auto& action)
        {
            using T = std::decay_t<decltype(action)>;

            if constexpr (std::is_same_v<T, CreatureWanderIntent>)
            {
                const std::uint32_t x = Grid::Step(creature.placement.x, action.dx);
                const std::uint32_t y = Grid::Step(creature.placement.y, action.dy);
                MoveCreature(creature, x, y);
                creature.m_ai.CompleteWanderStep();
            }
            else if constexpr (std::is_same_v<T, CreatureChaseIntent>)
            {
                const std::int32_t dx = action.target_placement.x < creature.placement.x   ? -1
                                        : action.target_placement.x > creature.placement.x ? 1
                                                                                           : 0;
                const std::int32_t dy = action.target_placement.y < creature.placement.y   ? -1
                                        : action.target_placement.y > creature.placement.y ? 1
                                                                                           : 0;
                MoveCreature(creature, Grid::Step(creature.placement.x, dx),
                             Grid::Step(creature.placement.y, dy));
            }
            else
            {
                const Player* target = GetPlayer(action.target_id);
                if (!target || target->character.hp == 0)
                    return;

                m_events.Publish(CreatureAttackRequestedEvent{
                    creature.instance_id,
                    action.target_id,
                    creature.placement.x,
                    creature.placement.y,
                    target->character.placement.x,
                    target->character.placement.y});
            }
        },
        intent);
}

bool Map::MoveCreature(Creature& creature, std::uint32_t x, std::uint32_t y,
                       std::uint32_t movementMode)
{
    if (!Grid::Contains(x, y) || creature.placement.IsAt(Placement{x, y}))
        return false;

    const std::uint32_t fromX = creature.placement.x;
    const std::uint32_t fromY = creature.placement.y;
    const Placement destination{x, y};

    creature.placement.direction = creature.placement.DirectionTo(destination);
    creature.placement.x = x;
    creature.placement.y = y;

    m_events.Publish(
        CreatureMoveEvent{creature.instance_id, fromX, fromY, x, y, movementMode});
    return true;
}
