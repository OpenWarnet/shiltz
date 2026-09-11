#pragma once

class GameData;
class Map;
class Outbox;
struct CharacterMoveEvent;
struct CharacterZoneChangeEvent;

// Updates a character's view when its zone changes and acknowledges walks.
class MovementSystem
{
public:
    // Registers on map's events in send order.
    MovementSystem(Map& map, const Outbox& outbox, const GameData& data);

    MovementSystem(const MovementSystem&) = delete;
    MovementSystem& operator=(const MovementSystem&) = delete;

    // Sends creatures in zones that entered the view (all of them on placement), then drops those that left it.
    void SendViewChange(const CharacterZoneChangeEvent& event) const;

    // GC_CHAR_MOVE back to the character that walked.
    void SendCharMove(const CharacterMoveEvent& event) const;

private:
    Map& m_map;
    const Outbox& m_outbox;
    const GameData& m_data;
};
