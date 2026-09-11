#pragma once

class GameData;
class Map;
class Outbox;
struct CharacterMoveEvent;

// Reacts to CharacterMoveEvent: updates the mover's view and acknowledges walks.
class MovementSystem
{
public:
    // Registers on map's events in send order.
    MovementSystem(Map& map, const Outbox& outbox, const GameData& data);

    MovementSystem(const MovementSystem&) = delete;
    MovementSystem& operator=(const MovementSystem&) = delete;

    // Sends creatures in zones the client hasn't been sent yet (all of them right after joining).
    void SendCrtLoad(const CharacterMoveEvent& event);

    // Tells the client to drop creatures in zones that left its view.
    void SendViewRemoveAll(const CharacterMoveEvent& event) const;

    // GC_CHAR_MOVE back to a character that walked; placements aren't acknowledged.
    void SendCharMove(const CharacterMoveEvent& event) const;

private:
    Map& m_map;
    const Outbox& m_outbox;
    const GameData& m_data;
};
