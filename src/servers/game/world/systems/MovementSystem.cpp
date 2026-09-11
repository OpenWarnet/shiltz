#include "MovementSystem.h"

#include "Outbox.h"
#include "protocol/server/CharMoveUpdate.h"
#include "protocol/server/CrtLoad.h"
#include "protocol/server/ViewRemoveAll.h"
#include "world/Map.h"
#include "world/MapEvents.h"
#include "world/Player.h"
#include "world/Zone.h"

#include <cstdint>

MovementSystem::MovementSystem(Map& map, const Outbox& outbox, const GameData& data)
    : m_map(map), m_outbox(outbox), m_data(data)
{
    map.Events().On<CharacterZoneChangeEvent>().Register<&MovementSystem::SendViewChange>(*this);
    map.Events().On<CharacterMoveEvent>().Register<&MovementSystem::SendCharMove>(*this);
}

void MovementSystem::SendViewChange(const CharacterZoneChangeEvent& event) const
{
    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    CrtLoad load;
    for (const auto& zone : Zone::Around(event.to))
    {
        // Already in view before the change.
        if (event.from && Zone::IsNeighboring(*event.from, zone))
            continue;

        for (const auto& creature : m_map.CreaturesInZone(zone))
        {
            load.records.push_back(CrtLoadRecord{
                .id = creature.instance_id,
                .x = creature.x,
                .y = creature.y,
                .monster_id = static_cast<std::uint32_t>(creature.monster_id),
                .direction = creature.direction,
                .hp = static_cast<std::uint64_t>(creature.hp),
            });
        }
    }

    // Always answer a placement (as the old enter flow did); zone changes only when something appeared.
    if (!event.from || !load.records.empty())
        m_outbox.Send(player->connection, load);

    // A placement has no previous view to clear.
    if (!event.from)
        return;

    ViewRemoveAll remove;
    for (const auto& zone : Zone::Around(*event.from))
    {
        // Still in view after the change.
        if (Zone::IsNeighboring(event.to, zone))
            continue;

        for (const auto& creature : m_map.CreaturesInZone(zone))
            remove.creature_ids.push_back(creature.instance_id);
    }

    if (!remove.creature_ids.empty())
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
    m_outbox.Send(player->connection, response);
}
