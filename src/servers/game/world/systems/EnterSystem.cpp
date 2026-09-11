#include "EnterSystem.h"

#include "Outbox.h"
#include "protocol/server/CharacterDataLoad.h"
#include "protocol/server/InventoryItemList.h"
#include "world/Map.h"
#include "world/MapEvents.h"
#include "world/Player.h"

#include <cstdint>
#include <ctime>

EnterSystem::EnterSystem(Map& map, const Outbox& outbox, const GameData& data)
    : m_map(map), m_outbox(outbox), m_data(data)
{
    map.Events().On<CharacterJoinEvent>().Register<&EnterSystem::SendCharacterDataLoad>(*this);
    map.Events().On<CharacterJoinEvent>().Register<&EnterSystem::SendInventoryItemList>(*this);
}

void EnterSystem::SendCharacterDataLoad(const CharacterJoinEvent& event) const
{
    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    m_outbox.Send(player->socket,
                  player->character.ToCharacterDataLoad(
                      /*epsUserFlag=*/1, // TODO: no DB column -- kept as the prior hardcoded placeholder
                      static_cast<std::uint32_t>(std::time(nullptr))));
}

void EnterSystem::SendInventoryItemList(const CharacterJoinEvent& event) const
{
    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    m_outbox.Send(player->socket, player->character.ToInventoryItemList());
}
