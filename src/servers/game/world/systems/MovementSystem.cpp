#include "MovementSystem.h"

#include "Outbox.h"
#include "protocol/server/CharMoveUpdate.h"
#include "protocol/server/CharNew.h"
#include "protocol/server/CharOtherLoad.h"
#include "protocol/server/CharRemove.h"
#include "protocol/server/CrtLoad.h"
#include "protocol/server/CrtMove.h"
#include "protocol/server/CrtNew.h"
#include "protocol/server/CrtRemove.h"
#include "protocol/server/ViewRemoveAll.h"
#include "world/Creature.h"
#include "world/Map.h"
#include "world/Player.h"
#include "world/events/CharacterEvents.h"
#include "world/events/CreatureEvents.h"
#include "world/Zone.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{
// Player::visible_players is kept sorted, so membership is a binary search.
bool Contains(const std::vector<std::uint32_t>& loaded, std::uint32_t id)
{
    return std::binary_search(loaded.begin(), loaded.end(), id);
}

// False if it was already there.
bool Insert(std::vector<std::uint32_t>& loaded, std::uint32_t id)
{
    const auto at = std::lower_bound(loaded.begin(), loaded.end(), id);
    if (at != loaded.end() && *at == id)
        return false;

    loaded.insert(at, id);
    return true;
}

// False if it wasn't there.
bool Erase(std::vector<std::uint32_t>& loaded, std::uint32_t id)
{
    const auto at = std::lower_bound(loaded.begin(), loaded.end(), id);
    if (at == loaded.end() || *at != id)
        return false;

    loaded.erase(at);
    return true;
}

CrtLoadRecord BuildCrtRecord(const Creature& creature)
{
    return CrtLoadRecord{
        .id = creature.instance_id,
        .x = creature.placement.x,
        .y = creature.placement.y,
        .monster_id = static_cast<std::uint32_t>(creature.monster_template.get().id),
        .direction = static_cast<std::uint32_t>(creature.placement.direction),
        .hp = static_cast<std::uint64_t>(creature.hp),
    };
}

// How another client sees this character (GC_CHAR_NEW / GC_CHAR_OTHER_LOAD). A file-local helper
// rather than a Character method -- built twice below (a newly-visible other player, and this
// viewer appearing to others), so worth naming once rather than inlining twice.
CharOtherRecord BuildCharOtherRecord(const Character& character)
{
    CharOtherRecord result;
    result.id = character.instance_id;
    result.name = character.name;
    result.x = character.placement.x;
    result.y = character.placement.y;
    result.level = static_cast<std::uint32_t>(character.level);
    result.job_id = character.job_id;
    result.gender = character.gender;
    result.hairstyle_id = character.hairstyle_id;
    result.face_id = character.face_id;
    result.max_hp = static_cast<std::uint32_t>(character.stats.derived.max_hp);
    result.hp = character.hp;
    result.direction = static_cast<std::uint32_t>(character.placement.direction);

    for (const auto& equipped : character.equipment)
    {
        if (equipped.slot >= CharOtherRecord::kEquipmentSlots)
            continue;

        result.equipment[equipped.slot] = {
            .item_id = equipped.item.item_id,
            .qty_or_refine = equipped.item.WireQuantityOrRefine(),
            .option_bits = equipped.item.option_bits,
        };
    }

    return result;
}
} // namespace

MovementSystem::MovementSystem(Map& map, const Outbox& outbox, const GameData& data)
    : m_map(map), m_outbox(outbox), m_data(data)
{
    map.Events().On<CharacterZoneChangeEvent>().Register<&MovementSystem::SendViewChange>(*this);
    map.Events().On<CharacterMoveEvent>().Register<&MovementSystem::SendCharMove>(*this);
    map.Events().On<CharacterLeaveEvent>().Register<&MovementSystem::SendCharRemove>(*this);
    map.Events().On<CreatureMoveEvent>().Register<&MovementSystem::SendCreatureMove>(*this);
    map.Events().On<CreatureRespawnEvent>().Register<&MovementSystem::SendCreatureRespawn>(*this);
}

void MovementSystem::SendViewChange(const CharacterZoneChangeEvent& event) const
{
    Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    ViewRemoveAll remove;
    std::vector<CharOtherRecord> arrived;
    SyncVisiblePlayers(*player, arrived, remove.player_ids);

    CharOtherLoad otherLoad;
    for (CharOtherRecord& record : arrived)
    {
        otherLoad.records.push_back(std::move(record));
        if (otherLoad.records.size() == CharOtherLoad::kMaxRecords)
        {
            m_outbox.Send(player->connection, otherLoad);
            otherLoad.records.clear();
        }
    }

    if (!otherLoad.records.empty())
        m_outbox.Send(player->connection, otherLoad);

    CrtLoad load;
    m_map.ForEachCreature(
        [&](const Creature& creature)
        {
            if (creature.hp <= 0)
                return;

            const Zone::Coordinates creatureZone =
                Zone::Of(creature.placement.x, creature.placement.y);
            if (!Zone::IsNeighboring(event.to, creatureZone) ||
                (event.from && Zone::IsNeighboring(*event.from, creatureZone)))
                return;

            load.records.push_back(BuildCrtRecord(creature));
        });

    // Always answer a placement (as the old enter flow did); zone changes only when something appeared.
    if (!event.from || !load.records.empty())
        m_outbox.Send(player->connection, load);

    // A placement has no previous view to clear creatures from.
    if (event.from)
    {
        m_map.ForEachCreature(
            [&](const Creature& creature)
            {
                const Zone::Coordinates creatureZone =
                    Zone::Of(creature.placement.x, creature.placement.y);
                if (Zone::IsNeighboring(*event.from, creatureZone) &&
                    !Zone::IsNeighboring(event.to, creatureZone))
                {
                    remove.creature_ids.push_back(creature.instance_id);
                }
            });
    }

    if (!remove.player_ids.empty() || !remove.creature_ids.empty())
        m_outbox.Send(player->connection, remove);
}

void MovementSystem::SendCharMove(const CharacterMoveEvent& event) const
{
    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    CharMoveUpdate response;
    response.user_instance_id = event.instance_id;
    response.direction = event.direction;
    response.x = event.to_x;
    response.y = event.to_y;
    response.speed = event.speed;
    response.stop_direction = event.stop_direction;

    // The mover plus every client that has it loaded.
    std::vector<ConnectionId> viewers{player->connection};
    for (const std::uint32_t id : player->visible_players)
    {
        if (const Player* viewer = m_map.GetPlayer(id))
            viewers.push_back(viewer->connection);
    }

    m_outbox.Send(viewers, response);
}

void MovementSystem::SendCharRemove(const CharacterLeaveEvent& event) const
{
    std::vector<ConnectionId> viewers;
    m_map.ForEachPlayer(
        [&](Player& viewer)
        {
            if (Erase(viewer.visible_players, event.instance_id))
                viewers.push_back(viewer.connection);
        });

    CharRemove remove;
    remove.id = event.instance_id;
    m_outbox.Send(viewers, remove);
}

void MovementSystem::SendCreatureMove(const CreatureMoveEvent& event) const
{
    const Zone::Coordinates fromZone = Zone::Of(event.from_x, event.from_y);
    const Zone::Coordinates toZone = Zone::Of(event.to_x, event.to_y);

    CrtMove move;
    move.creature_id = event.creature_id;
    move.x = event.from_x;
    move.y = event.from_y;
    move.target_x = event.to_x;
    move.target_y = event.to_y;
    move.movement_mode = event.movement_mode;

    if (fromZone == toZone)
    {
        m_outbox.Send(m_map.ViewersOf(toZone), move);
        return;
    }

    std::vector<ConnectionId> remained;
    std::vector<ConnectionId> arrived;
    std::vector<ConnectionId> departed;
    m_map.ForEachPlayer(
        [&](Player& player)
        {
            const Zone::Coordinates playerZone =
                Zone::Of(player.character.placement.x, player.character.placement.y);
            const bool wasInView = Zone::IsNeighboring(playerZone, fromZone);
            const bool isInView = Zone::IsNeighboring(playerZone, toZone);

            if (wasInView && isInView)
                remained.push_back(player.connection);
            else if (isInView)
                arrived.push_back(player.connection);
            else if (wasInView)
                departed.push_back(player.connection);
        });

    m_outbox.Send(remained, move);

    CrtRemove remove;
    remove.creature_id = event.creature_id;
    m_outbox.Send(departed, remove);

    const Creature* creature = m_map.GetCreature(event.creature_id);
    if (creature && creature->hp > 0)
    {
        CrtNew appeared;
        appeared.record = BuildCrtRecord(*creature);
        m_outbox.Send(arrived, appeared);
    }
}

void MovementSystem::SendCreatureRespawn(const CreatureRespawnEvent& event) const
{
    const Creature* creature = m_map.GetCreature(event.creature_id);
    if (!creature || creature->hp <= 0)
        return;

    CrtNew appeared;
    appeared.record = BuildCrtRecord(*creature);
    m_outbox.Send(m_map.ViewersOf(Zone::Of(creature->placement.x, creature->placement.y)),
                  appeared);
}

void MovementSystem::SyncVisiblePlayers(Player& viewer, std::vector<CharOtherRecord>& arrived,
                                        std::vector<std::uint32_t>& departed) const
{
    const std::uint32_t viewerId = viewer.character.instance_id;
    const Zone::Coordinates viewerZone =
        Zone::Of(viewer.character.placement.x, viewer.character.placement.y);
    const auto inView = [&](const Player& other)
    {
        return Zone::IsNeighboring(
            Zone::Of(other.character.placement.x, other.character.placement.y), viewerZone);
    };

    // Loaded characters that are out of view or no longer on this map.
    std::vector<ConnectionId> lostSight;
    std::erase_if(viewer.visible_players,
                  [&](std::uint32_t id)
                  {
                      Player* other = m_map.GetPlayer(id);
                      if (other && inView(*other))
                          return false;

                      departed.push_back(id);
                      if (other && Erase(other->visible_players, viewerId))
                          lostSight.push_back(other->connection);
                      return true;
                  });

    // Characters in view that this client hasn't loaded yet.
    std::vector<ConnectionId> gainedSight;
    m_map.ForEachPlayer(
        [&](Player& other)
        {
            const std::uint32_t otherId = other.character.instance_id;
            if (otherId == viewerId || Contains(viewer.visible_players, otherId) || !inView(other))
                return;

            Insert(viewer.visible_players, otherId);
            arrived.push_back(BuildCharOtherRecord(other.character));
            if (Insert(other.visible_players, viewerId))
                gainedSight.push_back(other.connection);
        });

    CharRemove removed;
    removed.id = viewerId;
    m_outbox.Send(lostSight, removed);

    if (!gainedSight.empty())
    {
        CharNew appeared;
        appeared.record = BuildCharOtherRecord(viewer.character);
        m_outbox.Send(gainedSight, appeared);
    }
}
