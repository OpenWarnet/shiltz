#pragma once

#include "../parser/MapScr.h"
#include "GroundItem.h"
#include "Player.h"
#include "Zone.h"
#include "world/common/EventBus.h"
#include "world/common/Pool.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>
#include <vector>

class Map
{
public:
    static constexpr std::int32_t kGridSize = 512;
    static constexpr std::int32_t kZoneSize = Zone::kSize;
    static constexpr std::int32_t kZoneGridSize = kGridSize / kZoneSize;
    static constexpr std::size_t kZoneCount =
        static_cast<std::size_t>(kZoneGridSize) * kZoneGridSize;

    std::int64_t id = 0;

    using CreatureMove = Zone::CreatureMove;

    explicit Map(MapRecord record);

    void AddItem(GroundItem item);

    bool RemoveItem(std::uint32_t id);

    std::optional<GroundItem> TryTakeItem(std::uint32_t id);

    std::vector<GroundItem> Items() const; // snapshot copy, safe from any thread.

    void AddCreature(Creature creature);

    std::vector<Creature> CreaturesInZone(std::int32_t zoneX, std::int32_t zoneY) const;

    std::vector<std::pair<std::int32_t, std::int32_t>> ZonesAround(std::int32_t x,
                                                                   std::int32_t y) const;

    // Adds player to the pool and publishes CharacterJoinEvent; false if already here. World strand only.
    [[nodiscard]] bool Spawn(Player player);

    // Removes and returns the player (to save it, or Spawn it on another
    // map); nullopt if it isn't on this map. World strand only.
    std::optional<Player> Despawn(std::uint32_t instanceId);

    // nullptr if that player isn't on this map. World strand only.
    Player* GetPlayer(std::uint32_t instanceId);
    const Player* GetPlayer(std::uint32_t instanceId) const;

    // World strand only.
    bool HasPlayers() const;

    static std::pair<std::int32_t, std::int32_t> ZoneOf(std::int32_t x, std::int32_t y);

    // Register listeners and publish this map's events; dispatched in Tick.
    EventBus& Events();

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

    // Keyed by character.instance_id. Not locked -- world strand only.
    Pool<std::uint32_t, Player> m_players;

    EventBus m_events;
};
