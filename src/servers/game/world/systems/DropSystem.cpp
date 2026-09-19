#include "DropSystem.h"

#include "Outbox.h"
#include "protocol/server/ItemMapNew.h"
#include "protocol/server/ItemMapRemove.h"
#include "protocol/server/ViewRemoveAll.h"
#include "world/Drop.h"
#include "world/Map.h"
#include "world/Player.h"
#include "world/Zone.h"
#include "world/events/CharacterEvents.h"
#include "world/events/DropEvents.h"

DropSystem::DropSystem(Map& map, const Outbox& outbox, const GameData& data)
    : m_map(map), m_outbox(outbox), m_data(data)
{
    map.Events().On<DropAddEvent>().Register<&DropSystem::SendItemMapNew>(*this);
    map.Events().On<DropRemoveEvent>().Register<&DropSystem::SendItemMapRemove>(*this);
    map.Events().On<CharacterZoneChangeEvent>().Register<&DropSystem::SendViewChange>(*this);
}

void DropSystem::SendItemMapNew(const DropAddEvent& event) const
{
    ItemMapNew message;
    message.id = event.id;
    message.x = event.x;
    message.y = event.y;
    message.item_id = event.item_id;
    message.owner_id = 0; // no ownership/loot-protection concept yet
    m_outbox.Send(m_map.ViewersOf(Zone::Of(event.x, event.y)), message, &Player::connection);
}

void DropSystem::SendItemMapRemove(const DropRemoveEvent& event) const
{
    ItemMapRemove message;
    message.id = event.id;
    m_outbox.Send(m_map.ViewersOf(Zone::Of(event.x, event.y)), message, &Player::connection);
}

void DropSystem::SendViewChange(const CharacterZoneChangeEvent& event) const
{
    Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    ViewRemoveAll remove; // item_ids only -- MovementSystem sends its own ViewRemoveAll for this same event
    m_map.ForEachDrop(
        [&](const Drop& drop)
        {
            const Zone::Coordinates zone = Zone::Of(drop.x, drop.y);
            const bool wasInView = event.from && Zone::IsNeighboring(*event.from, zone);
            const bool isInView = Zone::IsNeighboring(event.to, zone);

            if (isInView && !wasInView)
            {
                ItemMapNew message;
                message.id = drop.id;
                message.x = drop.x;
                message.y = drop.y;
                message.item_id = drop.item.item_id;
                message.owner_id = 0;
                m_outbox.Send(player->connection, message);
            }
            else if (wasInView && !isInView)
            {
                remove.item_ids.push_back(drop.id);
            }
        });

    if (!remove.item_ids.empty())
        m_outbox.Send(player->connection, remove);
}
