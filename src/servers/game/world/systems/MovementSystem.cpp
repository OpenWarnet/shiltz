#include "MovementSystem.h"

#include "Outbox.h"
#include "protocol/server/CharMoveUpdate.h"
#include "protocol/server/CrtLoad.h"
#include "protocol/server/ViewRemoveAll.h"
#include "tables/GameData.h"
#include "tables/MonsterTable.h"
#include "world/Map.h"
#include "world/MapEvents.h"
#include "world/Player.h"

#include <algorithm>
#include <cstdint>

namespace
{
bool Contains(const std::vector<std::pair<std::int32_t, std::int32_t>>& zones,
              const std::pair<std::int32_t, std::int32_t>& zone)
{
    return std::find(zones.begin(), zones.end(), zone) != zones.end();
}
} // namespace

MovementSystem::MovementSystem(Map& map, const Outbox& outbox, const GameData& data)
    : m_map(map), m_outbox(outbox), m_data(data)
{
    map.Events().On<CharacterMoveEvent>().Register<&MovementSystem::SendCrtLoad>(*this);
    map.Events().On<CharacterMoveEvent>().Register<&MovementSystem::SendViewRemoveAll>(*this);
    map.Events().On<CharacterMoveEvent>().Register<&MovementSystem::SendCharMove>(*this);
}

void MovementSystem::SendCrtLoad(const CharacterMoveEvent& event)
{
    Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    auto& knownZones = player->character.known_zones;
    const bool firstView = knownZones.empty();
    const auto zones = m_map.ZonesAround(event.to_x, event.to_y);

    CrtLoad response;
    for (const auto& zone : zones)
    {
        if (Contains(knownZones, zone))
            continue;

        for (const auto& creature : m_map.CreaturesInZone(zone.first, zone.second))
        {
            const MonsterRecord* monsterRecord = m_data.monsters.Find(creature.monster_id);

            response.records.push_back(CrtLoadRecord{
                .id = creature.instance_id,
                .x = static_cast<std::uint32_t>(creature.x),
                .y = static_cast<std::uint32_t>(creature.y),
                .monster_id = static_cast<std::uint32_t>(creature.monster_id),
                .direction = static_cast<std::uint32_t>(creature.direction),
                .hp = monsterRecord ? static_cast<std::uint64_t>(monsterRecord->hp) : 0,
            });
        }
    }

    knownZones = zones;

    // Always answer the first view (as the old enter flow did); moves only when something appeared.
    if (firstView || !response.records.empty())
        m_outbox.Send(player->connection, response);
}

void MovementSystem::SendViewRemoveAll(const CharacterMoveEvent& event) const
{
    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    const auto newZones = m_map.ZonesAround(event.to_x, event.to_y);

    ViewRemoveAll response;
    for (const auto& zone : m_map.ZonesAround(event.from_x, event.from_y))
    {
        if (Contains(newZones, zone))
            continue;

        for (const auto& creature : m_map.CreaturesInZone(zone.first, zone.second))
            response.creature_ids.push_back(creature.instance_id);
    }

    if (!response.creature_ids.empty())
        m_outbox.Send(player->connection, response);
}

void MovementSystem::SendCharMove(const CharacterMoveEvent& event) const
{
    if (!event.walk)
        return;

    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    CharMoveUpdate response;
    response.user_instance_id = event.instance_id;
    response.direction = event.walk->direction;
    response.x = static_cast<std::uint32_t>(event.to_x);
    response.y = static_cast<std::uint32_t>(event.to_y);
    response.speed = event.walk->speed;
    response.stop_direction = event.walk->stop_direction;
    m_outbox.Send(player->connection, response);
}
