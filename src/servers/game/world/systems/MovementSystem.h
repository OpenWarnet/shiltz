#pragma once

#include <cstdint>
#include <vector>

class GameData;
class Map;
class Outbox;
struct CharOtherRecord;
struct CharacterLeaveEvent;
struct CharacterMoveEvent;
struct CharacterZoneChangeEvent;
struct Player;

// Keeps each client's view of creatures and other characters current, and broadcasts walks to whoever can see them.
class MovementSystem
{
public:
    // Registers on map's events in send order.
    MovementSystem(Map& map, const Outbox& outbox, const GameData& data);

    MovementSystem(const MovementSystem&) = delete;
    MovementSystem& operator=(const MovementSystem&) = delete;

    // Loads characters and creatures that came into view (all of them on placement), then drops those that left it.
    void SendViewChange(const CharacterZoneChangeEvent& event) const;

    // GC_CHAR_MOVE to the walker and every client that has it loaded.
    void SendCharMove(const CharacterMoveEvent& event) const;

    // GC_CHAR_REMOVE to every client that had the departed character loaded.
    void SendCharRemove(const CharacterLeaveEvent& event) const;

private:
    // Matches viewer.visible_players to who is in view now, sending the other side of each change directly.
    void SyncVisiblePlayers(Player& viewer, std::vector<CharOtherRecord>& arrived,
                            std::vector<std::uint32_t>& departed) const;

    Map& m_map;
    const Outbox& m_outbox;
    const GameData& m_data;
};
