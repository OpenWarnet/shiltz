#include "Creature.h"

#include "tables/MonsterTable.h"
#include "world/common/EntityIdGenerator.h"
#include "world/common/EventBus.h"
#include "world/events/CombatEvents.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kIdleBaseDelay = 2000ms;
constexpr std::chrono::milliseconds kIdleMaxJitter = 1000ms;
constexpr std::chrono::milliseconds kWanderBaseDelay = 5000ms;
constexpr std::chrono::milliseconds kWanderMaxJitter = 1000ms;

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> kWanderOffsets{{
    {-1, -1},
    {0, -1},
    {1, -1},
    {-1, 0},
    {1, 0},
    {-1, 1},
    {0, 1},
    {1, 1},
}};
} // namespace

std::pair<std::uint32_t, std::uint32_t>
CreatureSpawn::RollPosition(std::int64_t monsterId, std::uint32_t radius,
                            std::uint32_t maxCoordinate)
{
    const std::uint64_t roll = MixSpawnSeed(monsterId);
    return {
        RollCoordinate(x, radius, maxCoordinate, roll),
        RollCoordinate(y, radius, maxCoordinate, roll >> 32),
    };
}

std::uint64_t CreatureSpawn::MixSpawnSeed(std::int64_t monsterId)
{
    std::uint64_t seed = static_cast<std::uint64_t>(monsterId);
    seed ^= static_cast<std::uint64_t>(x) << 32;
    seed ^= static_cast<std::uint64_t>(y) << 16;
    seed ^= sequence++;
    seed += 0x9E3779B97F4A7C15ULL;
    seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ULL;
    seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBULL;
    return seed ^ (seed >> 31);
}

std::uint32_t CreatureSpawn::RollCoordinate(std::uint32_t anchor, std::uint32_t radius,
                                            std::uint32_t maxCoordinate, std::uint64_t roll)
{
    const std::uint32_t minimum = anchor > radius ? anchor - radius : 0;
    const std::uint32_t maximum =
        radius > maxCoordinate - anchor ? maxCoordinate : anchor + radius;
    const std::uint64_t count = static_cast<std::uint64_t>(maximum) - minimum + 1;
    return minimum + static_cast<std::uint32_t>(roll % count);
}

Creature::Creature(CreatureKind creatureKind, std::int64_t monsterId,
                   const MonsterTable& monsters, std::optional<CreatureSpawn> creatureSpawn)
    : instance_id(EntityIdGenerator::Next()), kind(creatureKind),
      monster_template(RequireMonsterTemplate(monsters, monsterId)),
      hp(monster_template.get().hp), spawn(std::move(creatureSpawn))
{
}

void Creature::Tick(std::chrono::milliseconds delta, std::uint32_t maxCoordinate)
{
    if (kind != CreatureKind::Monster)
        return;

    if (lifecycle != CreatureLifecycle::Alive)
    {
        TickRespawn(delta);
        return;
    }

    TickAi(delta, maxCoordinate);
}

void Creature::Bind(EventBus& events) noexcept
{
    m_events = &events;
}

std::optional<DamageResult> Creature::TakeDamage(std::uint32_t requestedDamage,
                                                 std::uint32_t killerId)
{
    if (!m_events || kind != CreatureKind::Monster ||
        lifecycle != CreatureLifecycle::Alive || hp <= 0)
    {
        return std::nullopt;
    }

    const std::int64_t appliedDamage =
        std::min<std::int64_t>(static_cast<std::int64_t>(requestedDamage), hp);
    hp -= appliedDamage;

    if (hp == 0)
        return Kill(killerId, static_cast<std::uint32_t>(appliedDamage));

    return DamageResult{
        .damage = static_cast<std::uint32_t>(appliedDamage),
        .target_hp = static_cast<std::uint64_t>(hp),
    };
}

DamageResult Creature::Kill(std::uint32_t killerId, std::uint32_t damage)
{
    // The transition is deliberately idempotent: even an accidental second
    // call cannot publish another kill, expose the reward twice, or restart
    // an already-running respawn.
    if (lifecycle != CreatureLifecycle::Alive)
        return {};

    hp = 0;
    ai_state = CreatureAiState::Idle;
    ai_timer = std::chrono::milliseconds::zero();
    m_pendingMove.reset();
    ArmRespawn();

    const std::int64_t expReward = std::max<std::int64_t>(0, monster_template.get().exp_reward);
    m_events->Publish(MonsterKilledEvent{
        .monster_id = instance_id,
        .killer_id = killerId,
        .x = placement.x,
        .y = placement.y,
        .exp_reward = expReward,
    });

    return DamageResult{
        .damage = damage,
        .target_hp = 0,
        .exp_reward = expReward,
        .killed = true,
    };
}

void Creature::ArmRespawn()
{
    // A monster placed by NpcScr has no anchor to return to, and respawn_time 0 means gone for good.
    const std::int64_t respawnSeconds = monster_template.get().respawn_time;
    if (!spawn || respawnSeconds <= 0)
    {
        if (spawn)
            spawn->time_until_respawn.reset();

        lifecycle = CreatureLifecycle::Dead;
        return;
    }

    spawn->time_until_respawn = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds(respawnSeconds));
    lifecycle = CreatureLifecycle::WaitingForRespawn;
}

void Creature::Respawn(std::uint32_t maxCoordinate)
{
    lifecycle = CreatureLifecycle::Alive;
    hp = monster_template.get().hp;
    ai_state = CreatureAiState::Idle;
    ai_timer = std::chrono::milliseconds::zero();
    ai_decision_seq = 0;
    m_pendingMove.reset();

    if (!spawn)
        return;

    spawn->time_until_respawn.reset();

    const std::uint32_t radius = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
        monster_template.get().spawn_scatter_range, 0, maxCoordinate));
    const auto [spawnX, spawnY] =
        spawn->RollPosition(monster_template.get().id, radius, maxCoordinate);

    placement.x = spawnX;
    placement.y = spawnY;
    placement.direction = static_cast<Direction>(spawn->direction);
}

bool Creature::IsAlive() const noexcept
{
    return lifecycle == CreatureLifecycle::Alive;
}

bool Creature::NeedsRespawn() const noexcept
{
    return lifecycle == CreatureLifecycle::RespawnPending;
}

std::optional<CreatureMove> Creature::TakePendingMove()
{
    std::optional<CreatureMove> move = std::move(m_pendingMove);
    m_pendingMove.reset();
    return move;
}

void Creature::TickAi(std::chrono::milliseconds delta, std::uint32_t maxCoordinate)
{
    if (ai_timer > delta)
    {
        ai_timer -= delta;
        return;
    }

    RollNextAiState(maxCoordinate);
}

void Creature::TickRespawn(std::chrono::milliseconds delta)
{
    // Only a timer Kill armed can move -- Dead is terminal and RespawnPending already waits on Map.
    if (lifecycle != CreatureLifecycle::WaitingForRespawn || !spawn ||
        !spawn->time_until_respawn)
    {
        return;
    }

    if (*spawn->time_until_respawn > delta)
    {
        *spawn->time_until_respawn -= delta;
        return;
    }

    *spawn->time_until_respawn = std::chrono::milliseconds::zero();
    lifecycle = CreatureLifecycle::RespawnPending;
}

void Creature::RollNextAiState(std::uint32_t maxCoordinate)
{
    const std::uint32_t fromX = placement.x;
    const std::uint32_t fromY = placement.y;
    const std::uint64_t roll = MixDecisionSeed();

    if (roll & 1)
    {
        const auto [dx, dy] = kWanderOffsets[(roll >> 1) % kWanderOffsets.size()];
        placement.x = StepCoordinate(placement.x, dx, maxCoordinate);
        placement.y = StepCoordinate(placement.y, dy, maxCoordinate);

        ai_state = CreatureAiState::Wander;
        ai_timer =
            kWanderBaseDelay + std::chrono::milliseconds((roll >> 4) % kWanderMaxJitter.count());
    }
    else
    {
        ai_state = CreatureAiState::Idle;
        ai_timer =
            kIdleBaseDelay + std::chrono::milliseconds((roll >> 4) % kIdleMaxJitter.count());
    }

    if (placement.x != fromX || placement.y != fromY)
    {
        m_pendingMove = CreatureMove{
            .creature_id = instance_id,
            .from_x = fromX,
            .from_y = fromY,
            .to_x = placement.x,
            .to_y = placement.y,
        };
    }
}

std::uint64_t Creature::MixDecisionSeed()
{
    std::uint64_t seed =
        (static_cast<std::uint64_t>(instance_id) << 32) | ai_decision_seq++;
    seed += 0x9E3779B97F4A7C15ULL;
    seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ULL;
    seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBULL;
    return seed ^ (seed >> 31);
}

std::uint32_t Creature::StepCoordinate(std::uint32_t coordinate, std::int32_t delta,
                                       std::uint32_t maxCoordinate)
{
    return static_cast<std::uint32_t>(
        std::clamp<std::int64_t>(std::int64_t{coordinate} + delta, 0, maxCoordinate));
}

const MonsterRecord& Creature::RequireMonsterTemplate(const MonsterTable& monsters,
                                                       std::int64_t monsterId)
{
    const MonsterRecord* monsterTemplate = monsters.Find(monsterId);
    if (!monsterTemplate)
        throw std::runtime_error("monster template " + std::to_string(monsterId) + " not found");

    return *monsterTemplate;
}
