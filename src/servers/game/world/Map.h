#pragma once

#include "../parser/MapScr.h"
#include "GroundItem.h"
#include "Zone.h"

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
    static constexpr std::int32_t kZoneSize = Zone::kSize;
    static constexpr std::int32_t kZoneGridSize = kGridSize / kZoneSize;
    static constexpr std::size_t kZoneCount =
        static_cast<std::size_t>(kZoneGridSize) * kZoneGridSize;

    std::int64_t id = 0;

    using CreatureMove = Zone::CreatureMove;

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

    void Tick(std::chrono::milliseconds delta);

private:
    std::string monster_file;
    std::string npc_file;

    static std::vector<Zone> CreateZones();
    std::vector<CreatureMove> TickCreature(std::chrono::milliseconds delta);

    mutable std::mutex m_itemsMutex;
    std::vector<GroundItem> m_items;

    mutable std::shared_mutex m_zonesMutex;
    std::vector<Zone> m_zones;

    mutable std::mutex m_playersMutex;
    std::unordered_map<SOCKET, MapPlayer> m_players;
};
