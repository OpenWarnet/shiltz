#include "Map.h"

#include "Creature.h"
#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"
#include "world/common/Paths.h"

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

// The 8 tiles at Chebyshev distance 1 from a creature's own position --
// the "predetermined array of decisions" a Wander roll indexes into, so
// picking a target is a table lookup rather than re-deriving dx/dy from
// the random bits with a branch to reject (0, 0).
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

// Rolls this creature's next AI decision and applies it in place
// (state, timer, and -- for Wander -- position). Doesn't touch
// m_creatureGrid's bucketing; that's the caller's job once every
// creature in a zone has been visited (see Map::TickCreature).
void RollNextAiState(Creature& creature)
{
    const std::uint64_t roll = MixDecisionSeed(creature.instance_id, creature.ai_decision_seq++);

    // Low bit picks Idle vs. Wander (50/50 -- trivial to reweight
    // later); the rest of the bits are sliced off for whatever that
    // state needs, so one roll covers the whole decision.
    if (roll & 1)
    {
        const auto [dx, dy] = kWanderOffsets[(roll >> 1) % kWanderOffsets.size()];
        creature.x = std::clamp(creature.x + dx, 0, Map::kGridSize - 1);
        creature.y = std::clamp(creature.y + dy, 0, Map::kGridSize - 1);

        creature.ai_state = CreatureAiState::Wander;
        creature.ai_timer =
            kWanderBaseDelay + std::chrono::milliseconds((roll >> 4) % kWanderMaxJitter.count());
    }
    else
    {
        creature.ai_state = CreatureAiState::Idle;
        creature.ai_timer =
            kIdleBaseDelay + std::chrono::milliseconds((roll >> 4) % kIdleMaxJitter.count());
    }
}
} // namespace

Map::Map(MapRecord record, NextInstanceId nextInstanceId)
    : id(record.server_map_id), monster_file(std::move(record.monster_file)),
      npc_file(std::move(record.npc_file))
{
    for (const auto& spawn : NpcScr::Load(Paths::Data.npc_spawn / (record.npc_file + ".scr")))
    {
        for (const auto& instance : spawn.instances)
        {
            AddCreature(Creature{
                .instance_id = nextInstanceId(),
                .kind = CreatureKind::Npc,
                .monster_id = spawn.id,
                .x = instance.x,
                .y = instance.y,
                .direction = instance.direction,
            });
        }
    }

    for (const auto& group :
         MonsterSpawnScr::Load(Paths::Data.monster_spawn / (record.monster_file + ".scr")).groups)
    {
        for (const auto& instance : group.instances)
        {
            AddCreature(Creature{
                .instance_id = nextInstanceId(),
                .kind = CreatureKind::Monster,
                .monster_id = group.monster_id,
                .x = instance.x,
                .y = instance.y,
                .direction = instance.direction,
            });
        }
    }
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
        std::cout << "Ignoring creature " << creature.monster_id << " with out-of-range position ("
                  << creature.x << ", " << creature.y << ")\n";
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

void Map::SetPlayer(SOCKET socket, std::int32_t x, std::int32_t y)
{
    std::lock_guard lock(m_playersMutex);
    m_players[socket] = MapPlayer{.socket = socket, .x = x, .y = y};
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

bool Map::HasPlayers() const
{
    std::lock_guard lock(m_playersMutex);
    return !m_players.empty();
}

std::pair<std::int32_t, std::int32_t> Map::ZoneOf(std::int32_t x, std::int32_t y)
{
    return {x / kZoneSize, y / kZoneSize};
}

std::vector<Map::CreatureMove> Map::Tick(std::chrono::milliseconds delta)
{
    return TickCreature(delta);
}

std::vector<Map::CreatureMove> Map::TickCreature(std::chrono::milliseconds delta)
{
    // Creatures that ended this pass in a different zone than the bucket
    // they started it in -- collected while walking the grid below, then
    // re-added to their correct buckets afterward. Deferring the actual
    // relocation keeps the walk itself simple: every bucket is only ever
    // shrunk (via the erase-remove below) while it's the one being
    // visited, never grown out from under an in-flight iteration.
    std::vector<Creature> relocated;
    std::vector<CreatureMove> moves;

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

                RollNextAiState(creature);

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

            const auto stillBelongsHere = [zoneX, zoneY](const Creature& creature)
            { return ZoneOf(creature.x, creature.y) == std::pair{zoneX, zoneY}; };
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

    return moves;
}

std::vector<std::pair<std::int32_t, std::int32_t>> Map::ZonesAround(std::int32_t x,
                                                                    std::int32_t y) const
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
