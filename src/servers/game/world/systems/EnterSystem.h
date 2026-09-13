#pragma once

#include "world/systems/System.h"

class GameData;
class Map;
class Outbox;
struct CharacterJoinEvent;

// Sends a joining character its own state; creatures in view come from MovementSystem.
class EnterSystem : public System
{
public:
    // Registers on map's events in send order.
    EnterSystem(Map& map, const Outbox& outbox, const GameData& data);

    EnterSystem(const EnterSystem&) = delete;
    EnterSystem& operator=(const EnterSystem&) = delete;

    void SendCharacterDataLoad(const CharacterJoinEvent& event) const;
    void SendInventoryItemList(const CharacterJoinEvent& event) const;

private:
    Map& m_map;
    const Outbox& m_outbox;
    const GameData& m_data;
};
