#pragma once

#include "world/systems/System.h"

class GameData;
class Map;
class Outbox;
struct CharacterZoneChangeEvent;
struct DropAddEvent;
struct DropRemoveEvent;

// Broadcasts ground drops to nearby players -- on add/pickup/despawn, and on view changes
// (arriving in, or leaving, range of a drop already on the ground).
class DropSystem : public System
{
public:
    // Registers on map's events in send order.
    DropSystem(Map& map, const Outbox& outbox, const GameData& data);

    DropSystem(const DropSystem&) = delete;
    DropSystem& operator=(const DropSystem&) = delete;

    // GC_ITEM_MAP_NEW to every player near the drop.
    void SendItemMapNew(const DropAddEvent& event) const;

    // GC_ITEM_MAP_REMOVE to every player near the drop.
    void SendItemMapRemove(const DropRemoveEvent& event) const;

    // Loads drops that came into view (all of them on placement), then drops those that left it.
    void SendViewChange(const CharacterZoneChangeEvent& event) const;

private:
    Map& m_map;
    const Outbox& m_outbox;
    const GameData& m_data;
};
