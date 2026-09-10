#pragma once

#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"
#include "simulation/Components.h"
#include "world_v2/core/Map.h"
#include "world_v2/core/Module.h"
#include "world_v2/core/System.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <utility>

namespace game_sim
{

// Idle/wander timings and the eight steps a wander may take. Lifted
// unchanged from the v1 Map so a monster's behaviour is the same after the
// port as before it -- this is a relocation of the rules, not a rewrite.
inline constexpr std::int64_t kIdleBaseDelayMs = 2000;
inline constexpr std::int64_t kIdleMaxJitterMs = 1000;
inline constexpr std::int64_t kWanderBaseDelayMs = 5000;
inline constexpr std::int64_t kWanderMaxJitterMs = 1000;

inline constexpr std::array<std::pair<int, int>, 8> kWanderOffsets{{
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0}, {1, 0},
    {-1, 1}, {0, 1}, {1, 1},
}};

// SplitMix64's finalizer. Same (networkId, decisionSeq) always yields the
// same roll, so a creature's wander is reproducible and depends on nothing
// outside its own two arguments -- no shared RNG to own, seed or lock.
inline std::uint64_t MixDecisionSeed(std::uint32_t networkId, std::uint32_t decisionSeq)
{
    std::uint64_t x = (static_cast<std::uint64_t>(networkId) << 32) | decisionSeq;
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

// Idle monsters, wandering one tile at a time.
//
// A faithful port of the v1 Map::TickCreature/RollNextAiState pair, not an
// adoption of world_v2's own AISystem. That one hunts and attacks, which
// would be a behaviour change smuggled in under a refactor -- monsters that
// used to mill about would start chasing players. AISystem is installable
// here the day that is actually wanted.
//
// One roll decides both the next state and its duration: the low bit picks
// idle or wander, the rest of the bits are sliced off for the direction and
// the jitter.
class CreatureWanderSystem : public world_v2::ISystem
{
public:
    const char* Name() const override
    {
        return "CreatureWanderSystem";
    }

    void Run(world_v2::Map& world, float deltaSeconds) override
    {
        Update(world, static_cast<std::int64_t>(deltaSeconds * 1000.0f));
    }

    void Update(world_v2::Map& world, std::int64_t deltaMs)
    {
        world.registry.view<CreatureAiComponent, world_v2::GridPositionComponent, NetworkIdComponent>().Each(
            [&](world_v2::Entity entity, CreatureAiComponent& ai, world_v2::GridPositionComponent& position,
                NetworkIdComponent& network)
            {
                if (ai.timerMs > deltaMs)
                {
                    ai.timerMs -= deltaMs;
                    return;
                }

                const std::uint64_t roll = MixDecisionSeed(network.id, ai.decisionSeq++);

                if ((roll & 1) == 0)
                {
                    ai.state = CreatureAiState::Idle;
                    ai.timerMs = kIdleBaseDelayMs + static_cast<std::int64_t>((roll >> 4) % kIdleMaxJitterMs);
                    return;
                }

                const auto [offsetX, offsetY] = kWanderOffsets[(roll >> 1) % kWanderOffsets.size()];
                const int toX = std::clamp(position.x + offsetX, 0, world.tiles.Width() - 1);
                const int toY = std::clamp(position.y + offsetY, 0, world.tiles.Height() - 1);

                ai.state = CreatureAiState::Wander;
                ai.timerMs = kWanderBaseDelayMs + static_cast<std::int64_t>((roll >> 4) % kWanderMaxJitterMs);

                if (toX == position.x && toY == position.y)
                {
                    return;
                }

                // Routed through Tile::Move so the position and the
                // occupancy index cannot half-apply. No event is emitted:
                // the outbound layer notices movement by diffing positions,
                // not by listening -- see ViewModule.
                if (world.tiles.Move(entity, position.x, position.y, toX, toY))
                {
                    position.x = toX;
                    position.y = toY;
                }
            });
    }
};

// Places one map's NPCs and monsters, and wanders the monsters.
//
// Placement happens in Setup, from the same npcNN.scr and mNN.scr parsers
// the v1 MapLoader used -- the files and their meaning have not changed,
// only what gets built out of them. Every instance becomes an entity with a
// position, a template id and a network id; monsters additionally get the
// wander component.
//
// There is no respawning, because there was none before: a creature is
// placed once at load and stays. world_v2's SpawnerComponent/SpawnSystem
// would give respawning for free, but turning it on is a gameplay change
// and belongs in its own step.
class CreatureModule : public world_v2::Module
{
public:
    using NetworkIdAllocator = std::function<std::uint32_t()>;

    CreatureModule(std::filesystem::path npcScrPath, std::filesystem::path monsterScrPath,
                   NetworkIdAllocator allocate)
        : m_npcScrPath(std::move(npcScrPath))
        , m_monsterScrPath(std::move(monsterScrPath))
        , m_allocate(std::move(allocate))
    {
    }

    const char* Name() const override
    {
        return "Creatures";
    }

    void Setup(world_v2::ModuleContext& context) override
    {
        world_v2::Map& world = context.World();

        for (const auto& spawn : NpcScr::Load(m_npcScrPath))
        {
            for (const auto& instance : spawn.instances)
            {
                Place(world, spawn.id, CreatureKind::Npc, instance.x, instance.y, instance.direction);
            }
        }

        for (const auto& group : MonsterSpawnScr::Load(m_monsterScrPath).groups)
        {
            for (const auto& instance : group.instances)
            {
                Place(world, group.monster_id, CreatureKind::Monster, instance.x, instance.y, instance.direction);
            }
        }

        context.AddSystem<CreatureWanderSystem>();
    }

private:
    void Place(world_v2::Map& world, std::int64_t templateId, CreatureKind kind, std::int32_t x, std::int32_t y,
               std::int32_t direction)
    {
        const world_v2::Entity creature = world.Spawn(static_cast<int>(x), static_cast<int>(y));
        if (creature == world_v2::kNullEntity)
        {
            // A .scr row placing something off the map. Skipped rather than
            // fatal, the same way World::Start skips a map whose spawn file
            // is missing -- one bad row should not take a map down.
            return;
        }

        world.registry.Assign<CreatureComponent>(creature, templateId, kind, direction);
        world.registry.Assign<NetworkIdComponent>(creature, m_allocate());

        if (kind == CreatureKind::Monster)
        {
            world.registry.Assign<CreatureAiComponent>(creature, CreatureAiState::Idle, std::int64_t{0},
                                                       std::uint32_t{0});
        }
    }

    std::filesystem::path m_npcScrPath;
    std::filesystem::path m_monsterScrPath;
    NetworkIdAllocator m_allocate;
};

} // namespace game_sim
