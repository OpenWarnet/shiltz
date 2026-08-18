#include "Map.h"

#include <algorithm>
#include <array>
#include <iostream>

namespace
{
    std::pair<std::int32_t, std::int32_t> ZoneOf(std::int32_t x, std::int32_t y)
    {
        return Map::ZoneOf(x, y);
    }

    bool InZoneGrid(std::int32_t zoneX, std::int32_t zoneY)
    {
        return zoneX >= 0 && zoneX < Map::kZoneGridSize && zoneY >= 0 && zoneY < Map::kZoneGridSize;
    }

    std::size_t ZoneIndex(std::int32_t zoneX, std::int32_t zoneY)
    {
        return static_cast<std::size_t>(zoneY) * static_cast<std::size_t>(Map::kZoneGridSize) +
               static_cast<std::size_t>(zoneX);
    }

    using namespace std::chrono_literals;

    constexpr std::chrono::milliseconds kIdleBaseDelay = 2000ms;
    constexpr std::chrono::milliseconds kIdleMaxJitter = 1000ms;
    constexpr std::chrono::milliseconds kWanderBaseDelay = 5000ms;
    constexpr std::chrono::milliseconds kWanderMaxJitter = 1000ms;

    // Delay between consecutive steps of the same wander bout -- roughly
    // "1 grid per second", separate from kWanderBaseDelay (the delay
    // before the *next* Idle/Wander decision once a bout ends).
    constexpr std::chrono::milliseconds kWanderStepDelay = 1000ms;

    // How often an Attacking creature already within attack_range
    // rechecks its target's distance (e.g. the player backed off) --
    // actually resolving/broadcasting the attack itself isn't implemented
    // yet, so this is just the recheck cadence, not an attack cadence.
    constexpr std::chrono::milliseconds kAttackRecheckDelay = 1000ms;

    // The 8 tiles at Chebyshev distance 1 from a creature's own position --
    // the "predetermined array of decisions" a Wander roll indexes into, so
    // picking a target is a table lookup rather than re-deriving dx/dy from
    // the random bits with a branch to reject (0, 0).
    constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> kWanderOffsets{{
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0}, {1, 0},
        {-1, 1}, {0, 1}, {1, 1},
    }};

    // SplitMix64's finalizer: three xor-shifts and two multiplies, no
    // branches, no shared or per-creature RNG state to own/seed/lock --
    // just two integers in, one well-mixed 64-bit value out. Same
    // (instanceId, decisionSeq) always yields the same roll, which is
    // exactly what makes this safe to call from any of World's map-pool
    // threads at once: nothing about it depends on call order, thread, or
    // any state outside its own arguments. Not cryptographic, but "looks
    // random enough for idle NPC flavor" was never a high bar.
    std::uint64_t MixDecisionSeed(std::uint32_t instanceId, std::uint32_t decisionSeq)
    {
        std::uint64_t x = (static_cast<std::uint64_t>(instanceId) << 32) | decisionSeq;
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    }

    // Moves one tile in the direction roll picks. Shared by both "start a
    // wander bout" and "continue one" below -- what differs between those
    // two callers is how wander_steps_remaining/ai_timer get set
    // afterward, not the movement itself. Always moves along
    // creature.wander_direction_index -- picked fresh only when a bout
    // starts (see AdvanceAiState below) and left alone for every step
    // after that, so a multi-step bout is a straight walk in one heading
    // rather than a fresh random direction (and a zigzag) every step.
    void StepWander(Creature& creature)
    {
        const auto [dx, dy] = kWanderOffsets[creature.wander_direction_index];
        creature.x = std::clamp(creature.x + dx, 0, Map::kGridSize - 1);
        creature.y = std::clamp(creature.y + dy, 0, Map::kGridSize - 1);
        creature.ai_state = CreatureAiState::Wander;
    }

    // Rolls this creature's next AI decision and applies it in place
    // (state, timer, and -- for Wander -- position). Doesn't touch
    // m_creatureGrid's bucketing; that's the caller's job once every
    // creature in a zone has been visited (see Map::TickCreature).
    //
    // A creature already mid-wander-bout (wander_steps_remaining > 0) just
    // takes its next step instead of re-rolling Idle vs. Wander -- that's
    // what lets monster.scr's wander_step_count move a creature more than
    // one grid tile per decision, each tile still its own CreatureMove/
    // GC_CRT_MOVE broadcast (see Map::TickCreature). ai_state is
    // (re)stamped Wander on every step, including continued ones -- if
    // AttackCreature retagged this creature as Attacking mid-bout,
    // resuming the bout overwrites that the same way a fresh Idle/Wander
    // roll already does (see Creature.h's ai_target_id comment).
    void AdvanceAiState(Creature& creature)
    {
        const std::uint64_t roll = MixDecisionSeed(creature.instance_id, creature.ai_decision_seq++);

        if (creature.wander_steps_remaining > 0)
        {
            StepWander(creature);
            --creature.wander_steps_remaining;
            creature.ai_timer = creature.wander_steps_remaining > 0
                                     ? kWanderStepDelay
                                     : kWanderBaseDelay + std::chrono::milliseconds(
                                                               (roll >> 4) % kWanderMaxJitter.count());
            return;
        }

        // Low bit picks Idle vs. Wander (50/50 -- trivial to reweight
        // later); the rest of the bits are sliced off for whatever that
        // state needs, so one roll covers the whole decision.
        if (roll & 1)
        {
            creature.wander_direction_index =
                static_cast<std::uint8_t>((roll >> 1) % kWanderOffsets.size());
            StepWander(creature);

            // wander_step_count is this bout's max TOTAL steps, this one
            // included -- wander_steps_remaining only counts the ones
            // after it, so a fresh bout stores a value in [0, maxSteps -
            // 1] here, not [0, maxSteps]. No monster.scr data (or a
            // recorded 0) keeps the old single-step-per-decision behavior.
            // creature.monster is denormalized onto the Creature itself at
            // spawn time (see MapLoader::Load), so this needs no table
            // lookup here.
            const std::uint32_t maxSteps = creature.monster.wander_step_count > 0
                                                ? static_cast<std::uint32_t>(creature.monster.wander_step_count)
                                                : 1;
            creature.wander_steps_remaining =
                maxSteps > 1 ? static_cast<std::uint32_t>((roll >> 32) % maxSteps) : 0;

            creature.ai_timer = creature.wander_steps_remaining > 0
                                     ? kWanderStepDelay
                                     : kWanderBaseDelay + std::chrono::milliseconds(
                                                               (roll >> 4) % kWanderMaxJitter.count());
        }
        else
        {
            creature.ai_state = CreatureAiState::Idle;
            creature.ai_timer =
                kIdleBaseDelay + std::chrono::milliseconds((roll >> 4) % kIdleMaxJitter.count());
        }
    }

    // The wire's 1-based, 8-direction compass encoding (same one
    // CG_ATTACK_TO_CRT's own `direction` field uses) for whichever of
    // kWanderOffsets' 8 unit vectors (dx, dy) best matches -- each
    // component collapsed to its sign first, so any (dx, dy) (not just
    // unit steps) resolves to the nearest of the 8 headings. Falls back to
    // heading 1 for (0, 0) (attacker and target already share a tile),
    // which isn't itself one of the 8 entries.
    std::uint32_t FacingDirection(std::int32_t dx, std::int32_t dy)
    {
        const std::pair<std::int32_t, std::int32_t> sign{dx < 0 ? -1 : (dx > 0 ? 1 : 0),
                                                           dy < 0 ? -1 : (dy > 0 ? 1 : 0)};
        for (std::size_t i = 0; i < kWanderOffsets.size(); ++i)
        {
            if (kWanderOffsets[i] == sign)
                return static_cast<std::uint32_t>(i) + 1;
        }
        return 1;
    }

    // Advances a creature currently tagged Attacking (see
    // Map::AttackCreature): chases `targetPos` one tile at a time --
    // clamping each axis's delta to [-1, 1], the Chebyshev-optimal step
    // with no pathfinding, same movement granularity as a Wander step --
    // until within monster.attack_range (Chebyshev distance). Once in
    // range, faces the target and swings on a fixed cadence -- returns the
    // swing as a Map::CreatureAttack for TickCreature to collect, always a
    // miss (no damage model exists yet, see handlers/Attack.cpp's own
    // comment on the player's own attack path). Falls back to Idle if
    // `targetPos` is empty (the target disconnected or left this map),
    // clearing ai_target_id the way a fresh Idle/Wander roll otherwise
    // would have (see Creature.h's ai_target_id comment).
    std::optional<Map::CreatureAttack>
    AdvanceAttackState(Creature& creature,
                        const std::optional<std::pair<std::int32_t, std::int32_t>>& targetPos)
    {
        if (!targetPos)
        {
            creature.ai_state = CreatureAiState::Idle;
            creature.ai_target_id = 0;
            creature.wander_steps_remaining = 0;
            creature.ai_timer = kIdleBaseDelay;
            return std::nullopt;
        }

        const std::int32_t dx = targetPos->first - creature.x;
        const std::int32_t dy = targetPos->second - creature.y;
        // Explicit template argument dodges the Windows.h min/max macro
        // collision -- see Inventory.cpp/HexDump.h for the same idiom;
        // this project doesn't define NOMINMAX anywhere.
        const std::int32_t chebyshevDistance =
            std::max<std::int32_t>(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);

        const std::int32_t attackRange = creature.monster.attack_range > 0
                                              ? static_cast<std::int32_t>(creature.monster.attack_range)
                                              : 1;
        if (chebyshevDistance <= attackRange)
        {
            creature.direction = static_cast<std::int32_t>(FacingDirection(dx, dy));
            creature.ai_timer = kAttackRecheckDelay;
            return Map::CreatureAttack{
                .creature_id = creature.instance_id,
                .target_id = creature.ai_target_id,
                .direction = FacingDirection(dx, dy),
                .pos_x = static_cast<std::uint32_t>(creature.x),
                .pos_y = static_cast<std::uint32_t>(creature.y),
            };
        }

        creature.x = std::clamp(creature.x + std::clamp(dx, -1, 1), 0, Map::kGridSize - 1);
        creature.y = std::clamp(creature.y + std::clamp(dy, -1, 1), 0, Map::kGridSize - 1);
        creature.ai_timer = kWanderStepDelay;
        return std::nullopt;
    }
} // namespace

Map::Map(Map&& other)
{
    std::scoped_lock lock(other.m_itemsMutex, other.m_creatureGridMutex, other.m_playersMutex);
    m_items = std::move(other.m_items);
    m_creatureGrid = std::move(other.m_creatureGrid);
    m_players = std::move(other.m_players);
}

Map& Map::operator=(Map&& other)
{
    if (this != &other)
    {
        std::scoped_lock lock(m_itemsMutex, m_creatureGridMutex, m_playersMutex, other.m_itemsMutex,
                               other.m_creatureGridMutex, other.m_playersMutex);
        m_items = std::move(other.m_items);
        m_creatureGrid = std::move(other.m_creatureGrid);
        m_players = std::move(other.m_players);
    }
    return *this;
}

void Map::AddItem(GroundItem item)
{
    std::lock_guard lock(m_itemsMutex);
    m_items.push_back(item);
}

bool Map::RemoveItem(std::uint32_t id)
{
    std::lock_guard lock(m_itemsMutex);
    auto it = std::find_if(m_items.begin(), m_items.end(),
                            [id](const GroundItem& item) { return item.id == id; });
    if (it == m_items.end())
        return false;

    m_items.erase(it);
    return true;
}

std::optional<GroundItem> Map::TryTakeItem(std::uint32_t id)
{
    std::lock_guard lock(m_itemsMutex);
    auto it = std::find_if(m_items.begin(), m_items.end(),
                            [id](const GroundItem& item) { return item.id == id; });
    if (it == m_items.end())
        return std::nullopt;

    GroundItem taken = *it;
    m_items.erase(it);
    return taken;
}

std::vector<GroundItem> Map::Items() const
{
    std::lock_guard lock(m_itemsMutex);
    return m_items;
}

void Map::AddCreature(Creature creature)
{
    const auto [zoneX, zoneY] = ZoneOf(creature.x, creature.y);
    if (!InZoneGrid(zoneX, zoneY))
    {
        std::cout << "Ignoring creature " << creature.monster_id
                  << " with out-of-range position (" << creature.x << ", " << creature.y << ")\n";
        return;
    }

    std::unique_lock lock(m_creatureGridMutex);
    m_creatureGrid[ZoneIndex(zoneX, zoneY)].push_back(creature);
}

std::vector<Creature> Map::CreaturesInZone(std::int32_t zoneX, std::int32_t zoneY) const
{
    if (!InZoneGrid(zoneX, zoneY))
        return {};

    std::shared_lock lock(m_creatureGridMutex);
    return m_creatureGrid[ZoneIndex(zoneX, zoneY)];
}

void Map::SetPlayer(SOCKET socket, std::uint32_t instanceId, std::int32_t x, std::int32_t y)
{
    std::lock_guard lock(m_playersMutex);
    m_players[socket] = MapPlayer{.socket = socket, .instance_id = instanceId, .x = x, .y = y};
}

void Map::RemovePlayer(SOCKET socket)
{
    std::lock_guard lock(m_playersMutex);
    m_players.erase(socket);
}

std::vector<Map::MapPlayer> Map::Players() const
{
    std::vector<MapPlayer> players;

    std::lock_guard lock(m_playersMutex);
    players.reserve(m_players.size());
    for (const auto& [socket, player] : m_players)
        players.push_back(player);

    return players;
}

std::optional<std::pair<std::int32_t, std::int32_t>>
Map::FindPlayerPosition(std::uint32_t instanceId) const
{
    std::lock_guard lock(m_playersMutex);
    for (const auto& [socket, player] : m_players)
    {
        if (player.instance_id == instanceId)
            return std::pair{player.x, player.y};
    }

    return std::nullopt;
}

bool Map::HasPlayers() const
{
    std::lock_guard lock(m_playersMutex);
    return !m_players.empty();
}

std::pair<std::int32_t, std::int32_t> Map::ZoneOf(std::int32_t x, std::int32_t y)
{
    return {x / kZoneSize, y / kZoneSize};
}

Map::TickResult Map::Tick(std::chrono::milliseconds delta)
{
    return TickCreature(delta);
}

Map::TickResult Map::TickCreature(std::chrono::milliseconds delta)
{
    // Creatures that ended this pass in a different zone than the bucket
    // they started it in -- collected while walking the grid below, then
    // re-added to their correct buckets afterward. Deferring the actual
    // relocation keeps the walk itself simple: every bucket is only ever
    // shrunk (via the erase-remove below) while it's the one being
    // visited, never grown out from under an in-flight iteration.
    std::vector<Creature> relocated;
    std::vector<CreatureMove> moves;
    std::vector<CreatureAttack> attacks;

    std::unique_lock lock(m_creatureGridMutex);
    for (std::int32_t zoneY = 0; zoneY < kZoneGridSize; ++zoneY)
    {
        for (std::int32_t zoneX = 0; zoneX < kZoneGridSize; ++zoneX)
        {
            auto& bucket = m_creatureGrid[ZoneIndex(zoneX, zoneY)];

            for (Creature& creature : bucket)
            {
                if (creature.kind != CreatureKind::Monster)
                    continue; // NPCs (shops/dialogue/warp) never move.

                if (creature.ai_timer > delta)
                {
                    creature.ai_timer -= delta;
                    continue;
                }

                const std::int32_t fromX = creature.x;
                const std::int32_t fromY = creature.y;

                if (creature.ai_state == CreatureAiState::Attacking)
                {
                    if (auto attack = AdvanceAttackState(creature, FindPlayerPosition(creature.ai_target_id)))
                        attacks.push_back(*attack);
                }
                else
                {
                    AdvanceAiState(creature);
                }

                if (creature.x != fromX || creature.y != fromY)
                {
                    moves.push_back(CreatureMove{
                        .creature_id = creature.instance_id,
                        .from_x = fromX,
                        .from_y = fromY,
                        .to_x = creature.x,
                        .to_y = creature.y,
                    });
                }
            }

            const auto stillBelongsHere = [zoneX, zoneY](const Creature& creature) {
                return ZoneOf(creature.x, creature.y) == std::pair{zoneX, zoneY};
            };
            const auto misplaced = std::partition(bucket.begin(), bucket.end(), stillBelongsHere);
            relocated.insert(relocated.end(), std::make_move_iterator(misplaced),
                              std::make_move_iterator(bucket.end()));
            bucket.erase(misplaced, bucket.end());
        }
    }

    for (Creature& creature : relocated)
    {
        // Wander's own std::clamp already keeps (x, y) inside the grid, so
        // this is always a valid zone -- no InZoneGrid re-check needed.
        const auto [zoneX, zoneY] = ZoneOf(creature.x, creature.y);
        m_creatureGrid[ZoneIndex(zoneX, zoneY)].push_back(std::move(creature));
    }

    return TickResult{.moves = std::move(moves), .attacks = std::move(attacks)};
}

bool Map::AttackCreature(std::uint32_t creatureId, std::uint32_t attackerId)
{
    std::unique_lock lock(m_creatureGridMutex);
    for (auto& bucket : m_creatureGrid)
    {
        for (Creature& creature : bucket)
        {
            if (creature.kind != CreatureKind::Monster || creature.instance_id != creatureId)
                continue;

            creature.ai_state = CreatureAiState::Attacking;
            creature.ai_target_id = attackerId;
            return true;
        }
    }

    return false;
}

std::vector<std::pair<std::int32_t, std::int32_t>> Map::ZonesAround(std::int32_t x, std::int32_t y) const
{
    std::vector<std::pair<std::int32_t, std::int32_t>> zones;
    const auto [zoneX, zoneY] = ZoneOf(x, y);

    for (std::int32_t dy = -1; dy <= 1; ++dy)
    {
        for (std::int32_t dx = -1; dx <= 1; ++dx)
        {
            const std::int32_t neighborX = zoneX + dx;
            const std::int32_t neighborY = zoneY + dy;
            if (InZoneGrid(neighborX, neighborY))
                zones.emplace_back(neighborX, neighborY);
        }
    }

    return zones;
}
