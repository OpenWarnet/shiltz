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
#include <span>
#include <utility>
#include <vector>

class MonsterTable;

class Map
{
public:
    static constexpr std::uint32_t kGridSize = 512;
    static constexpr std::uint32_t kZoneGridSize = kGridSize / Zone::kSize;
    static constexpr std::size_t kZoneCount =
        static_cast<std::size_t>(kZoneGridSize) * kZoneGridSize;

    std::int64_t id = 0;

    // True if tile (x, y) is on the map; anything else must be rejected before it reaches the world.
    static bool IsInBounds(std::uint32_t x, std::uint32_t y) noexcept;

    // monsters seeds each spawned creature's instance stats; not retained.
    Map(MapRecord record, const MonsterTable& monsters);

    void AddItem(GroundItem item);

    bool RemoveItem(std::uint32_t id);

    std::optional<GroundItem> TryTakeItem(std::uint32_t id);

    std::vector<GroundItem> Items() const; // snapshot copy, safe from any thread.

    // World strand only.
    void AddCreature(Creature creature);

    // Invalidated by AddCreature and the next Tick. World strand only.
    std::span<const Creature> CreaturesInZone(Zone::Coordinates zone) const;

    // Adds player to the pool and publishes CharacterJoinEvent; false if already here. World strand only.
    [[nodiscard]] bool Spawn(Player player);

    // Removes and returns the player (to save it, or Spawn it on another map) and publishes
    // CharacterLeaveEvent; nullopt if it isn't on this map. World strand only.
    std::optional<Player> Despawn(std::uint32_t instanceId);

    // nullptr if that player isn't on this map. World strand only.
    Player* GetPlayer(std::uint32_t instanceId);
    const Player* GetPlayer(std::uint32_t instanceId) const;

    // World strand only.
    bool HasPlayers() const;

    // Calls fn(Player&) for every player on this map; don't Spawn or Despawn from fn. World strand only.
    template <typename Fn> void ForEachPlayer(Fn&& fn)
    {
        for (Player& player : m_players)
            fn(player);
    }

    // Register listeners and publish this map's events; dispatched in Tick.
    EventBus& Events();

    void Tick(std::chrono::milliseconds delta);

private:
    std::string monster_file;
    std::string npc_file;

    static std::vector<Zone> CreateZones();
    std::vector<Zone::CreatureMove> TickCreature(std::chrono::milliseconds delta);

    mutable std::mutex m_itemsMutex;
    std::vector<GroundItem> m_items;

    // Not locked -- world strand only.
    std::vector<Zone> m_zones;

    // Keyed by character.instance_id. Not locked -- world strand only.
    Pool<std::uint32_t, Player> m_players;

    EventBus m_events;
};
