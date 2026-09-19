#include "Creature.h"

#include "common/Random.h"
#include "tables/MonsterTable.h"
#include "world/common/EntityIdGenerator.h"
#include "world/common/EventBus.h"
#include "world/events/CombatEvents.h"
#include "world/events/CreatureEvents.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

std::pair<std::uint32_t, std::uint32_t> CreatureSpawn::RollPosition(std::uint32_t radius) const
{
    return {RollCoordinate(x, radius), RollCoordinate(y, radius)};
}

std::uint32_t CreatureSpawn::RollCoordinate(std::uint32_t anchor, std::uint32_t radius)
{
    const std::uint32_t minimum = anchor > radius ? anchor - radius : 0;
    const std::uint32_t maximum =
        radius > Grid::kMaxCoordinate - anchor ? Grid::kMaxCoordinate : anchor + radius;
    const std::uint64_t count = static_cast<std::uint64_t>(maximum) - minimum + 1;
    return minimum + static_cast<std::uint32_t>(Random::Below(count));
}

Creature::Creature(CreatureKind creatureKind, std::int64_t monsterId, const MonsterTable& monsters,
                   std::optional<CreatureSpawn> creatureSpawn)
    : instance_id(EntityIdGenerator::Next()), kind(creatureKind),
      monster_template(RequireMonsterTemplate(monsters, monsterId)), hp(monster_template.get().hp),
      spawn(std::move(creatureSpawn))
{
}

void Creature::Tick(std::chrono::milliseconds delta)
{
    if (kind != CreatureKind::Monster)
        return;

    if (!IsAlive())
        TickRespawn(delta);
}

void Creature::Bind(EventBus& events) noexcept
{
    m_events = &events;
}

std::optional<DamageResult> Creature::TakeDamage(std::uint32_t requestedDamage,
                                                 std::uint32_t killerId)
{
    if (!m_events || kind != CreatureKind::Monster || hp <= 0)
        return std::nullopt;

    const std::int64_t appliedDamage =
        std::min<std::int64_t>(static_cast<std::int64_t>(requestedDamage), hp);

    if (appliedDamage == hp)
        return Kill(killerId, static_cast<std::uint32_t>(appliedDamage));

    hp -= appliedDamage;

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
    if (hp <= 0)
        return {};

    hp = 0;
    m_ai.Reset();

    // A monster placed by NpcScr has no anchor to return to, and respawn_time 0 means gone for
    // good.
    const std::int64_t respawnSeconds = monster_template.get().respawn_time;
    if (spawn && respawnSeconds > 0)
    {
        spawn->time_until_respawn = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::seconds(respawnSeconds));
    }

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

void Creature::Respawn()
{
    hp = monster_template.get().hp;
    m_ai.Reset();

    if (!spawn)
        return;

    spawn->time_until_respawn.reset();

    const std::uint32_t radius = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
        monster_template.get().spawn_scatter_range, 0, Grid::kMaxCoordinate));
    const auto [spawnX, spawnY] = spawn->RollPosition(radius);

    placement.x = spawnX;
    placement.y = spawnY;
    placement.direction = static_cast<Direction>(spawn->direction);
}

bool Creature::IsAlive() const noexcept
{
    return hp > 0;
}

CreatureAiStateKind Creature::AiState() const noexcept
{
    return m_ai.State();
}

void Creature::TickRespawn(std::chrono::milliseconds delta)
{
    // Only a timer Kill armed can move -- without one this death is terminal.
    if (!m_events || !spawn || !spawn->time_until_respawn)
        return;

    if (*spawn->time_until_respawn > delta)
    {
        *spawn->time_until_respawn -= delta;
        return;
    }

    Respawn();
    m_events->Publish(CreatureRespawnEvent{.creature_id = instance_id});
}

const MonsterRecord& Creature::RequireMonsterTemplate(const MonsterTable& monsters,
                                                      std::int64_t monsterId)
{
    const MonsterRecord* monsterTemplate = monsters.Find(monsterId);
    if (!monsterTemplate)
        throw std::runtime_error("monster template " + std::to_string(monsterId) + " not found");

    return *monsterTemplate;
}
