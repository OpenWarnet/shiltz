#pragma once

#include "../parser/MapScr.h"
#include "Creature.h"
#include "GroundItem.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <winsock2.h>

class Map
{
public:
    using NextInstanceId = std::function<std::uint32_t()>;

    static constexpr std::int32_t kGridSize = 512;
    static constexpr std::int32_t kZoneSize = 16;
    static constexpr std::int32_t kZoneGridSize = kGridSize / kZoneSize;

    std::int64_t id = 0;

    struct CreatureMove
    {
        std::uint32_t creature_id = 0;
        std::int32_t from_x = 0;
        std::int32_t from_y = 0;
        std::int32_t to_x = 0;
        std::int32_t to_y = 0;
    };

    struct MapPlayer
    {
        SOCKET socket = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;
    };

    Map(MapRecord record, NextInstanceId nextInstanceId);

    void AddItem(GroundItem item);

    bool RemoveItem(std::uint32_t id);

    std::optional<GroundItem> TryTakeItem(std::uint32_t id);

    std::vector<GroundItem> Items() const; // snapshot copy, safe from any thread.

    void AddCreature(Creature creature);

    std::vector<Creature> CreaturesInZone(std::int32_t zoneX, std::int32_t zoneY) const;

    std::vector<std::pair<std::int32_t, std::int32_t>> ZonesAround(std::int32_t x,
                                                                   std::int32_t y) const;

    void SetPlayer(SOCKET socket, std::int32_t x, std::int32_t y);

    void RemovePlayer(SOCKET socket);

    std::vector<MapPlayer> Players() const;

    bool HasPlayers() const;

    static std::pair<std::int32_t, std::int32_t> ZoneOf(std::int32_t x, std::int32_t y);

    std::vector<CreatureMove> Tick(std::chrono::milliseconds delta);

private:
    std::string monster_file;
    std::string npc_file;

    std::vector<CreatureMove> TickCreature(std::chrono::milliseconds delta);

    mutable std::mutex m_itemsMutex;
    std::vector<GroundItem> m_items;

    mutable std::shared_mutex m_creatureGridMutex;
    std::vector<std::vector<Creature>> m_creatureGrid =
        std::vector<std::vector<Creature>>(static_cast<std::size_t>(kZoneGridSize) * kZoneGridSize);

    mutable std::mutex m_playersMutex;
    std::unordered_map<SOCKET, MapPlayer> m_players;
};
