#pragma once

#include "../parser/MapScr.h"
#include "Drop.h"
#include "Player.h"
#include "Zone.h"
#include "world/common/EventBus.h"
#include "world/common/Pool.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

class GameData;

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

    // data.monsters seeds each spawned creature's instance stats; data itself is retained (see
    // m_data) to recalculate dirty players' derived stats once per Tick.
    Map(MapRecord record, const GameData& data);

    // Everything below (Tick and Events aside) runs only on the world strand -- see
    // GameServer::ScheduleTick -- so none of it locks anything.

    void AddCreature(Creature creature);

    // Invalidated by AddCreature and the next Tick.
    std::span<const Creature> CreaturesInZone(Zone::Coordinates zone) const;

    // Adds player to the pool and publishes CharacterJoinEvent; false if already here.
    [[nodiscard]] bool Spawn(Player player);

    // Adds drop to the pool and publishes DropAddEvent.
    void Spawn(Drop drop);

    // Removes and returns the player (to save it, or Spawn it on another map) and publishes
    // CharacterLeaveEvent; nullopt if it isn't on this map.
    std::optional<Player> Despawn(const Player& player);

    // Removes and returns the drop (matched by Drop::id), publishing DropRemoveEvent; nullopt if
    // it wasn't here.
    std::optional<Drop> Despawn(const Drop& drop);

    // Moves player to (x, y), publishing CharacterMoveEvent (and CharacterZoneChangeEvent if the
    // move crosses into a different 3x3 view); false (no-op) if (x, y) is out of bounds.
    bool Move(Player& player, std::uint32_t x, std::uint32_t y, std::uint32_t direction,
              std::uint32_t speed, std::uint32_t stopDirection);

    // nullptr if that player isn't on this map.
    Player* GetPlayer(std::uint32_t instanceId);
    const Player* GetPlayer(std::uint32_t instanceId) const;

    bool HasPlayers() const;

    // Calls fn(Player&) for every player on this map; don't Spawn or Despawn from fn.
    template <typename Fn> void ForEachPlayer(Fn&& fn)
    {
        for (Player& player : m_players)
            fn(player);
    }

    // Calls fn(Drop&) for every drop on this map; don't Spawn or Despawn from fn.
    template <typename Fn> void ForEachDrop(Fn&& fn)
    {
        for (Drop& drop : m_drops)
            fn(drop);
    }

    // Register listeners and publish this map's events; dispatched in Tick.
    EventBus& Events();

    void Tick(std::chrono::milliseconds delta);

private:
    std::string monster_file;
    std::string npc_file;

    static std::vector<Zone> CreateZones();
    std::vector<Zone::CreatureMove> TickCreature(std::chrono::milliseconds delta);
    void TickDrops(std::chrono::milliseconds delta);

    // Per-player bookkeeping that isn't triggered by a specific event: today that's just
    // recalculating any player whose CharacterStats::dirty is set (see Character.h), clearing the
    // flag as it goes -- more will land here (regen, buff-duration countdown, ...). Runs first in
    // Tick, before m_events.Dispatch(), so anything that reads stats.derived while reacting to
    // this tick's events (e.g. EnterSystem on a same-tick join, MovementSystem's view broadcasts)
    // sees fresh values.
    void TickCharacter(std::chrono::milliseconds delta);

    std::vector<Zone> m_zones;

    // Keyed by character.instance_id.
    Pool<std::uint32_t, Player> m_players;

    // Keyed by Drop::id.
    Pool<std::uint32_t, Drop> m_drops;

    EventBus m_events;

    const GameData& m_data;
};
