// End-to-end exercise of the whole framework.
//
// Every other test in world_v2 pins down one piece. This one runs all of
// them together, for hundreds of ticks, on a map with walls: commands
// arriving from outside, spawners filling and refilling, monsters hunting
// through corridors, combat, deaths cascading into loot and experience, and
// notices going back out to viewers -- while checking after every single
// tick that the world is still internally consistent.
//
// The invariants are the point. Most of the ways this framework can go
// wrong are not a wrong answer to one question but a slow divergence
// between two things that are supposed to describe the same fact -- an
// entity's tile and the occupancy grid's idea of it, a spawner's count and
// the monsters that actually exist. Those drift silently and only surface
// much later as creatures standing inside each other or camps that never
// refill. Checking them every tick is what turns that into a test failure
// with a tick number on it.
//
// What each test here is for
// --------------------------
//   ColdTypeIdsAreHandedOutUniquely       the type-id counter, raced cold
//   TheWorldStaysConsistentUnderLoad      the broad run: every invariant, 300 ticks
//   ItemsChangeHandsWithoutMultiplying    drop and pickup, round trip
//   TwoPlayersRacingForOneItemGetOneCopy  the duplication guard, full tick
//   AFullBagLeavesTheLootWhereItFell      refusal does not destroy the item
//   TwoIdenticalRunsAgreeExactly          determinism
//   MapsTickInParallelWithoutInterference the one-registry-per-map claim
//   ALevelCurveAdvancesWithoutSticking    the game layer's half of level-up
//   AWalledInSpawnerNeverPlacesOrBreaks   a spawner that can never succeed
//   APlayerDyingMidRunIsHandledCleanly    death of a viewer, mid-run
//   CommandsNamingDeadEntitiesAreRejected the push-then-die window
//   CommandsFromManyThreadsSurviveALiveSimulation  the inbound seam
//   LootThatNobodyCollectsStopsPilingUp   despawn timers bound the floor
//   ALongRunDoesNotDrift                  soak: population bounded over time
//
// Broadcast correctness has no test of its own: it is audited every tick of
// every run above, in both directions -- nothing reached a viewer that was
// out of range, and no death in plain sight went unannounced. See
// CheckNoticesReachedTheRightViewers.
//
// What is deliberately NOT covered here, because world_v2 does not have it:
// there is no drop path in the framework. No DropItemRequestComponent, no
// drop system, no ItemDroppedEvent -- items enter the world as loot and
// leave it through PickupSystem, and that is the whole loop. The drop test
// below therefore drives the seam the way a game layer would have to today
// (RemoveItem plus SpawnGroundItem, from a command handler) rather than
// exercising a system that exists. If drop is meant to be first-class, it
// needs the same request/consume/announce shape as pickup, and this test
// is what would then be pointed at it instead.

#include "Simulation.h"
#include "component/Combat.h"
#include "component/Despawn.h"
#include "component/Grid.h"
#include "component/Items.h"
#include "component/Network.h"
#include "component/Request.h"
#include "component/Spawn.h"
#include "world/MapWorld.h"
#include "core/Entity.h"
#include "core/Test.h"
#include "event/CombatEvents.h"
#include "event/ItemEvents.h"
#include "event/LifecycleEvents.h"
#include "event/SpawnEvents.h"
#include "system/BroadcastSystem.h"
#include "system/CombatRules.h"
#include "system/DespawnRules.h"
#include "system/ItemRules.h"
#include "system/SpawnRules.h"
#include "world/Inventory.h"

#include "core/TypeId.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

using namespace world_v2;

namespace
{

constexpr int kMapSize = 96;
constexpr int kPlayerFaction = 1;
constexpr int kMonsterFaction = 2;
constexpr int kPlayerCount = 8;
constexpr int kSpawnerCount = 6;
constexpr int kMonstersPerSpawner = 5;
constexpr float kTickSeconds = 0.25f;

// Every item in this scenario is the same shape, so a stack limit is one
// number rather than a lookup. A real game reads both from item.scr.
constexpr std::uint32_t kLootMaxStack = 10;
constexpr std::uint32_t kLootPerCorpse = 3;

// Deterministic mixing, used both to drive the scenario and to digest it.
// Nothing in the simulation is random; the whole run has to be reproducible
// bit for bit or the determinism check below means nothing.
std::uint32_t Hash(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t value = a * 0x9E3779B9u ^ (b + 0x165667B1u + (a << 6) + (a >> 2));
    value ^= value >> 15;
    value *= 0x2545F491u;
    value ^= value >> 13;
    return value;
}

struct MoveCommand
{
    Entity entity = kNullEntity;
    int directionX = 0;
    int directionY = 0;
};

struct AttackCommand
{
    Entity attacker = kNullEntity;
    Entity target = kNullEntity;
    int damage = 0;
};

struct PickupCommand
{
    Entity picker = kNullEntity;
    Entity item = kNullEntity;
};

// There is no drop system, so this command is the game layer doing it by
// hand: take from the bag, put on the ground, and make sure a failure at
// the second step does not lose what the first step took. That "put it
// back" branch is the part a real DropSystem would exist to make
// unnecessary.
struct DropCommand
{
    Entity dropper = kNullEntity;
    std::uint32_t itemId = 0;
    std::uint32_t quantity = 0;
};

// The knobs the variant scenarios below turn. Everything defaults to the
// shape the original broad run had, so adding these changed no existing
// test's behaviour or digest.
struct Config
{
    // Players drop things back on the ground as well as picking them up.
    // Off by default: it changes the order stream, and the determinism and
    // load tests were calibrated without it.
    bool dropping = false;

    // Slots per player. Small enough and pickups start being refused,
    // which is the path that has to leave the loot lying there.
    std::size_t inventorySlots = 12;

    // Install the LevelUpEvent listener that raises the next threshold.
    // CombatRules deliberately does not: the curve is level.scr data the
    // game layer owns. Without one, requiredForNextLevel never moves.
    bool levelCurve = false;

    // Add a spawner sealed inside solid rock, which can never place
    // anything.
    bool walledInSpawner = false;

    // Players tough enough to outlast the run. Turned down when the point
    // is to watch them die.
    int playerHealth = 100000;

    // Seconds before unclaimed loot times out. Zero leaves it lying there
    // forever, which is the behaviour every other test here was calibrated
    // against, so this stays off by default.
    float lootLifetimeSeconds = 0.0f;

    // Whether players bother picking things up.
    //
    // Turning it off is how the flooding case is produced. The obvious way
    // -- give them no inventory slots -- does not work, and the way it
    // fails is worth recording: a player standing next to loot it cannot
    // hold issues a pickup command every tick, has it refused every tick,
    // and never gets round to attacking anything. Monsters stop dying,
    // loot stops dropping, and the scenario quietly measures nothing. A
    // real client would do the same, which makes it a genuine note for
    // whoever writes the pickup UI, but here it just has to not happen.
    bool collectLoot = true;
};

class Scenario
{
public:
    explicit Scenario(Config config = {})
        : m_config(config)
        , m_simulation(kMapSize, kMapSize, true)
    {
        CarveTerrain();
        WireCommands();

        // Before WireRules on purpose. The loss bookkeeping below reads a
        // dying player's inventory, and CombatRules' death handler is the
        // one that despawns them -- registering first is what puts this
        // ahead of it in the listener order. Test bookkeeping, not
        // framework behaviour: nothing in world_v2 depends on listener
        // order, because every event carries what its handlers need.
        WireNotices();

        WireRules();
        PlaceSpawners();
        PlacePlayers();

        m_simulation.PrimeSpawns();
    }

    // One tick: issue everyone's orders through the queue, advance, then
    // audit.
    void Step(int tick)
    {
        // Cleared before the tick, not after, so the audit can still see
        // what this tick produced.
        m_tickDeaths.clear();
        m_tickItemSpawns.clear();
        m_tickNotices.clear();

        IssuePlayerOrders(tick);
        m_simulation.Tick(kTickSeconds);
        CheckInvariants(tick);
        Digest();
    }

    std::uint64_t DigestValue() const
    {
        return m_digest;
    }

    std::size_t Deaths() const
    {
        return m_deaths;
    }

    std::size_t Spawns() const
    {
        return m_spawns;
    }

    std::size_t Notices() const
    {
        return m_notices;
    }

    std::size_t LootDrops() const
    {
        return m_lootDrops;
    }

    std::size_t Pickups() const
    {
        return m_pickups;
    }

    std::size_t Drops() const
    {
        return m_drops;
    }

    std::size_t LootTimeouts() const
    {
        return m_lootTimeouts;
    }

    // The most entities seen sharing one tile at any point in the run.
    // Asserted rather than merely reported: if this never exceeded one, the
    // scenario would be running under the old exclusive rule by accident
    // and every shared-tile invariant would be vacuous.
    std::size_t PeakTileOccupancy() const
    {
        return m_peakTileOccupancy;
    }

    std::size_t PickupsRefused(PickupFailure reason) const
    {
        return m_pickupFailures[static_cast<std::size_t>(reason)];
    }

    std::size_t LevelUps() const
    {
        return m_levelUps;
    }

    int Failures() const
    {
        return m_failures;
    }

    Simulation& Sim()
    {
        return m_simulation;
    }

    const std::vector<Entity>& Players() const
    {
        return m_players;
    }

    Entity WalledInSpawner() const
    {
        return m_walledInSpawner;
    }

private:
    void CarveTerrain()
    {
        // Vertical walls every eight columns, with a gap every sixteen
        // rows. Enough structure that monsters have to path around
        // something and that spawners can be walled into corners -- and
        // enough that "no entity ever stands on an unwalkable tile" is a
        // claim with teeth.
        for (int x = 8; x < kMapSize; x += 8)
        {
            for (int y = 0; y < kMapSize; ++y)
            {
                if (y % 16 != 0)
                {
                    m_simulation.World().tiles.SetWalkable(x, y, false);
                }
            }
        }
    }

    void WireCommands()
    {
        m_simulation.Commands().On<PickupCommand>(
            [](MapWorld& world, const PickupCommand& command)
            {
                if (!world.registry.Exists(command.picker) || !world.registry.Exists(command.item))
                {
                    return;
                }
                world.registry.Assign<PickupItemRequestComponent>(command.picker, command.item);
            });

        m_simulation.Commands().On<MoveCommand>(
            [](MapWorld& world, const MoveCommand& command)
            {
                // A player can have died between the push and the drain.
                if (!world.registry.Exists(command.entity))
                {
                    return;
                }
                world.registry.Assign<MoveIntentComponent>(command.entity, command.directionX, command.directionY);
            });

        m_simulation.Commands().On<AttackCommand>(
            [](MapWorld& world, const AttackCommand& command)
            {
                if (!world.registry.Exists(command.attacker) || !world.registry.Exists(command.target))
                {
                    return;
                }
                world.registry.Assign<AttackRequestComponent>(command.attacker, command.target, command.damage);
            });

        // Dropping, done the only way it can be done today: by hand, in a
        // command handler.
        //
        // This runs in stage 1, during the drain, which is why it may
        // create an entity at all -- nothing is iterating yet. A drop
        // attempted from inside a system sweep would have to be an event
        // instead, and that difference is exactly what a real DropSystem
        // would encapsulate.
        //
        // Note what has to be written out longhand here and would not be if
        // the framework owned it: taking from the bag and placing on the
        // ground are two steps that must both happen or neither, so the
        // failure branch has to hand the goods back. PickupSystem gets this
        // for free by ordering its own steps so that nothing after the
        // fallible part can fail.
        m_simulation.Commands().On<DropCommand>(
            [this](MapWorld& world, const DropCommand& command)
            {
                if (!world.registry.Exists(command.dropper))
                {
                    return;
                }

                InventoryComponent* inventory = world.registry.TryGet<InventoryComponent>(command.dropper);
                const GridPositionComponent* position =
                    world.registry.TryGet<GridPositionComponent>(command.dropper);
                if (inventory == nullptr || position == nullptr)
                {
                    return;
                }

                const std::uint32_t taken = RemoveItem(*inventory, command.itemId, command.quantity);
                if (taken == 0)
                {
                    return;
                }

                const Entity item =
                    SpawnGroundItem(world, command.itemId, taken, kLootMaxStack, position->x, position->y);
                if (item == kNullEntity)
                {
                    // Unreachable -- the dropper is standing on the map, so
                    // the tile is in bounds -- but silently vaporising what
                    // was already taken is the one outcome the conservation
                    // check could not tell apart from a framework bug.
                    TryAddItem(*inventory, command.itemId, taken, kLootMaxStack);
                    return;
                }

                // Not counted as creation: this is the same goods moving
                // from a bag to the floor. See the conservation check.
                ++m_drops;
                m_itemsRecirculated += taken;
            });
    }

    void WireRules()
    {
        InstallCombatRules(m_simulation.World());
        InstallItemRules(m_simulation.World());
        InstallDespawnRules(m_simulation.World());

        // LootDropEvent is the seam world_v2 deliberately leaves unhandled,
        // because turning a table id into items needs .scr data it does not
        // read. This is a game layer standing in for that: one item per
        // corpse, the table id doubling as the item id.
        m_simulation.World().events.Listen<LootDropEvent>(
            [this](const LootDropEvent& event)
            {
                const Entity item = SpawnGroundItem(m_simulation.World(), event.dropTableId, kLootPerCorpse,
                                                    kLootMaxStack, event.x, event.y);

                // Creation is counted here, at the one place in this
                // scenario where items come into existence from nothing --
                // not off GroundItemSpawnedEvent, which also fires for a
                // player putting something back down. Conflating the two
                // would make the conservation check below unable to tell a
                // duplication bug from an ordinary drop.
                if (item == kNullEntity)
                {
                    return;
                }

                m_itemsCreated += kLootPerCorpse;

                if (m_config.lootLifetimeSeconds > 0.0f)
                {
                    m_simulation.World().registry.Assign<DespawnTimerComponent>(item,
                                                                                m_config.lootLifetimeSeconds);
                }
            });

        // The other half of the level-up cascade. CombatRules raises the
        // level and stops; what the next level costs is level.scr data it
        // has no business inventing. Without this listener the threshold
        // never moves, every subsequent kill re-levels immediately, and the
        // kMaxLevelsPerAward guard quietly becomes load-bearing -- which is
        // the state the rest of these tests run in, on purpose, since that
        // guard is worth exercising too.
        if (m_config.levelCurve)
        {
            m_simulation.World().events.Listen<LevelUpEvent>(
                [this](const LevelUpEvent& event)
                {
                    ExperienceComponent* experience =
                        m_simulation.World().registry.TryGet<ExperienceComponent>(event.entity);
                    if (experience == nullptr)
                    {
                        return;
                    }

                    // Steep enough that one award cannot clear several
                    // levels, which is what makes "the cap was never
                    // reached" an assertion with content.
                    experience->requiredForNextLevel = std::uint64_t{120} * event.level * event.level;
                });
        }

        InstallSpawnRules(m_simulation.World(),
                          [](MapWorld& world, Entity monster, std::uint32_t templateId)
                          {
                              world.registry.Assign<FactionComponent>(monster, kMonsterFaction);
                              world.registry.Assign<HealthComponent>(monster, 24, 24);
                              world.registry.Assign<AIComponent>(monster, 7, 1, kNullEntity);
                              world.registry.Assign<AttackPowerComponent>(monster, 2);
                              world.registry.Assign<ExperienceRewardComponent>(monster, std::uint64_t{40});
                              world.registry.Assign<LootTableComponent>(monster, templateId);
                          });
    }

    void WireNotices()
    {
        m_simulation.OnNotice(
            [this](Entity viewer, const Notice& notice)
            {
                ++m_notices;

                // Kept for this tick only, so the audit can check what
                // reached whom -- both that nothing out of range arrived,
                // and that nothing in range was missed.
                m_tickNotices.push_back(DeliveredNotice{viewer, notice});

                // Fold the outbound stream into the digest too: two
                // identical runs must tell their viewers exactly the same
                // story, not merely end in the same state.
                Mix(static_cast<std::uint64_t>(notice.kind));
                Mix(notice.subject);
                Mix(static_cast<std::uint64_t>(notice.x));
                Mix(static_cast<std::uint64_t>(notice.y));
            });

        // Every ground item that appears, whatever put it there. Compared
        // against creations plus drops, so an item that materialises
        // without either is caught.
        m_simulation.World().events.Listen<GroundItemSpawnedEvent>(
            [this](const GroundItemSpawnedEvent& event)
            {
                m_itemsAnnounced += event.quantity;
                m_tickItemSpawns.push_back(RecordedAppearance{event.entity, event.x, event.y});
            });

        m_simulation.World().events.Listen<ItemPickedUpEvent>([this](const ItemPickedUpEvent&) { ++m_pickups; });

        // Registered before WireRules, like the death bookkeeping below and
        // for the same reason: InstallDespawnRules removes the entity, and
        // this has to read what was on it first.
        m_simulation.World().events.Listen<EntityExpiredEvent>(
            [this](const EntityExpiredEvent& event)
            {
                if (const GroundItemComponent* ground =
                        m_simulation.World().registry.TryGet<GroundItemComponent>(event.entity))
                {
                    m_itemsExpired += ground->quantity;
                    ++m_lootTimeouts;
                }
            });

        m_simulation.World().events.Listen<ItemPickupFailedEvent>(
            [this](const ItemPickupFailedEvent& event)
            { ++m_pickupFailures[static_cast<std::size_t>(event.reason)]; });

        m_simulation.World().events.Listen<DeathEvent>(
            [this](const DeathEvent& event)
            {
                ++m_deaths;
                m_tickDeaths.push_back(RecordedDeath{event.entity, event.x, event.y});

                // Anything a dying entity was carrying leaves the world with
                // it. Counted here so the conservation check below stays an
                // exact equality rather than an inequality that would hide a
                // duplication bug.
                if (const InventoryComponent* inventory =
                        m_simulation.World().registry.TryGet<InventoryComponent>(event.entity))
                {
                    for (const InventoryComponent::Slot& slot : inventory->slots)
                    {
                        m_itemsLost += slot.quantity;
                    }
                }
            });
        m_simulation.World().events.Listen<MonsterSpawnedEvent>([this](const MonsterSpawnedEvent&) { ++m_spawns; });
        m_simulation.World().events.Listen<LootDropEvent>([this](const LootDropEvent&) { ++m_lootDrops; });
        m_simulation.World().events.Listen<LevelUpEvent>([this](const LevelUpEvent&) { ++m_levelUps; });
    }

    void PlaceSpawners()
    {
        for (int i = 0; i < kSpawnerCount; ++i)
        {
            const int x = 3 + (i % 3) * 32;
            const int y = 8 + (i / 3) * 40;

            const Entity spawner = m_simulation.World().registry.Create();
            m_simulation.World().registry.Assign<SpawnerComponent>(
                spawner, static_cast<std::uint32_t>(500 + i), x, y, 3, kMonstersPerSpawner, 0, 2.0f, 0.0f,
                std::uint32_t{0});
            m_spawners.push_back(spawner);
        }

        if (!m_config.walledInSpawner)
        {
            return;
        }

        // A camp sealed in solid rock. SpawnRules drops a request whose
        // FindSpawnTile comes back empty and remembers nothing, so this
        // spawner asks again every respawn interval, forever, and never
        // succeeds. What must not happen is that it advances its sequence,
        // counts phantom monsters, or wedges the tick.
        const int wallX = kMapSize - 6;
        const int wallY = kMapSize - 6;
        for (int y = wallY - 4; y <= wallY + 4; ++y)
        {
            for (int x = wallX - 4; x <= wallX + 4; ++x)
            {
                m_simulation.World().tiles.SetWalkable(x, y, false);
            }
        }

        m_walledInSpawner = m_simulation.World().registry.Create();
        m_simulation.World().registry.Assign<SpawnerComponent>(m_walledInSpawner, std::uint32_t{999}, wallX, wallY, 2,
                                                               4, 0, 1.0f, 0.0f, std::uint32_t{0});
    }

    // Walks outward from (x, y) for somewhere a player can actually stand.
    // The terrain has walls in it, so a fixed grid of start positions puts
    // some of them inside one.
    //
    // The walkability test is explicit because Spawn no longer makes it:
    // placement succeeds anywhere in bounds, walls included, since that is
    // what loot against a wall needs. A caller that wants open ground has
    // to say so -- SpawnRules does the same thing through FindSpawnTile.
    // Occupancy is not consulted at all: standing where someone else is
    // already standing is fine.
    Entity SpawnNear(int x, int y)
    {
        for (int radius = 0; radius < kMapSize; ++radius)
        {
            for (int dy = -radius; dy <= radius; ++dy)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (!m_simulation.World().tiles.IsWalkable(x + dx, y + dy))
                    {
                        continue;
                    }

                    const Entity placed = m_simulation.World().Spawn(x + dx, y + dy);
                    if (placed != kNullEntity)
                    {
                        return placed;
                    }
                }
            }
        }

        return kNullEntity;
    }

    void PlacePlayers()
    {
        for (int i = 0; i < kPlayerCount; ++i)
        {
            const int x = 2 + (i % 4) * 2;
            const int y = 40 + (i / 4) * 2;

            const Entity player = SpawnNear(x, y);
            CHECK(player != kNullEntity);
            if (player == kNullEntity)
            {
                continue;
            }

            m_simulation.World().registry.Assign<FactionComponent>(player, kPlayerFaction);

            // Tough enough to survive the whole run, so the scenario keeps
            // producing traffic instead of going quiet halfway through.
            // Player death gets its own focused test below.
            m_simulation.World().registry.Assign<HealthComponent>(player, m_config.playerHealth,
                                                                  m_config.playerHealth);
            m_simulation.World().registry.Assign<ExperienceComponent>(player, std::uint64_t{0}, std::uint32_t{1},
                                                                      std::uint64_t{120});
            m_simulation.World().registry.Assign<ViewerComponent>(player, 12);
            m_simulation.World().registry.Assign<InventoryComponent>(player,
                                                                     MakeInventory(m_config.inventorySlots));
            m_players.push_back(player);
        }
    }

    // Everything a player does goes through CommandQueue, exactly as a
    // packet handler on a connection thread would -- the simulation is
    // never poked directly.
    void IssuePlayerOrders(int tick)
    {
        for (std::size_t i = 0; i < m_players.size(); ++i)
        {
            const Entity player = m_players[i];
            if (!m_simulation.World().registry.Exists(player))
            {
                continue;
            }

            const std::uint32_t roll = Hash(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(tick));

            // Occasionally put something back down before doing anything
            // else. Deliberately ahead of the pickup branch, so a dropped
            // stack is available to be picked up again on a later tick --
            // items churn between ground and bag instead of only ever
            // travelling one way.
            if (m_config.dropping && (roll % 7) == 0)
            {
                std::uint32_t heldId = 0;
                if (FirstHeldItem(player, heldId))
                {
                    m_simulation.Commands().Push(DropCommand{player, heldId, 1});
                    continue;
                }
            }

            // Grab loot underfoot first, then swing at whatever is
            // adjacent, then wander.
            const Entity loot = m_config.collectLoot ? NearbyItem(player) : kNullEntity;
            if (loot != kNullEntity)
            {
                m_simulation.Commands().Push(PickupCommand{player, loot});
                continue;
            }

            const Entity target = AdjacentEnemy(player);
            if (target != kNullEntity && (roll % 3) != 0)
            {
                m_simulation.Commands().Push(AttackCommand{player, target, 6});
                continue;
            }

            const int direction = static_cast<int>(roll % 9);
            m_simulation.Commands().Push(MoveCommand{player, (direction % 3) - 1, (direction / 3) - 1});
        }
    }

    // The first thing in a player's bag, if anything is. Written against
    // Inventory.h's stated invariant -- an unused slot is one with quantity
    // zero, never a missing element -- rather than against slot order.
    bool FirstHeldItem(Entity player, std::uint32_t& itemId)
    {
        const InventoryComponent* inventory =
            m_simulation.World().registry.TryGet<InventoryComponent>(player);
        if (inventory == nullptr)
        {
            return false;
        }

        for (const InventoryComponent::Slot& slot : inventory->slots)
        {
            if (!slot.IsEmpty())
            {
                itemId = slot.itemId;
                return true;
            }
        }

        return false;
    }

    // Ground items are not in the occupancy grid -- that is the point of
    // them -- so finding one means asking the registry rather than the map.
    Entity NearbyItem(Entity player)
    {
        const GridPositionComponent* position = m_simulation.World().registry.TryGet<GridPositionComponent>(player);
        if (position == nullptr)
        {
            return kNullEntity;
        }

        const int px = position->x;
        const int py = position->y;

        Entity found = kNullEntity;
        m_simulation.World().registry.view<GroundItemComponent, GridPositionComponent>().Each(
            [&](Entity item, GroundItemComponent& ground, GridPositionComponent& itemPosition)
            {
                if (found != kNullEntity || ground.quantity == 0)
                {
                    return;
                }

                const int dx = itemPosition.x - px;
                const int dy = itemPosition.y - py;
                if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1)
                {
                    found = item;
                }
            });

        return found;
    }

    Entity AdjacentEnemy(Entity player)
    {
        const GridPositionComponent* position = m_simulation.World().registry.TryGet<GridPositionComponent>(player);
        if (position == nullptr)
        {
            return kNullEntity;
        }

        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }

                // A tile holds any number of things now, so this walks the
                // list rather than reading one handle. Most of what it
                // finds is not a monster -- ground items are on the index
                // too -- which is why the faction check does the work.
                for (const Entity occupant :
                     m_simulation.World().tiles.OccupantsAt(position->x + dx, position->y + dy))
                {
                    const FactionComponent* faction =
                        m_simulation.World().registry.TryGet<FactionComponent>(occupant);
                    if (faction != nullptr && faction->faction == kMonsterFaction)
                    {
                        return occupant;
                    }
                }
            }
        }

        return kNullEntity;
    }

    // The audit. Runs after every tick; a failure names the tick it broke
    // on.
    void CheckInvariants(int tick)
    {
        MapWorld& world = m_simulation.World();

        // 1. Forward: everything the index lists at a tile is a living
        //    entity that agrees it is standing there. A stale listing is a
        //    phantom -- something the AI can see and attack at a square
        //    nothing is on.
        //
        //    Sharpened by shared tiles rather than weakened: the index now
        //    holds a list per tile, so this also catches an entity left in
        //    a list twice, which would let it be targeted twice and removed
        //    once.
        std::size_t listedEntries = 0;
        for (int y = 0; y < kMapSize; ++y)
        {
            for (int x = 0; x < kMapSize; ++x)
            {
                const std::size_t here = world.tiles.OccupantCount(x, y);
                if (here > m_peakTileOccupancy)
                {
                    m_peakTileOccupancy = here;
                }

                // The count array is a cache of the list's length, kept
                // because the AI scan needs a dense probe. A cache that
                // drifts low makes tiles invisible to targeting; one that
                // drifts high costs nothing but is the same bug. Checked
                // here because nothing inside TileGrid can.
                if (here != world.tiles.OccupantsAt(x, y).size())
                {
                    Fail(tick, "the tile occupant count disagrees with the tile list");
                }

                for (const Entity occupant : world.tiles.OccupantsAt(x, y))
                {
                    ++listedEntries;

                    if (!world.registry.Exists(occupant))
                    {
                        Fail(tick, "the tile index holds a destroyed entity");
                        continue;
                    }

                    const GridPositionComponent* position =
                        world.registry.TryGet<GridPositionComponent>(occupant);
                    if (position == nullptr || position->x != x || position->y != y)
                    {
                        Fail(tick, "tile index and GridPositionComponent disagree");
                    }

                    // Creatures may not stand in walls. Items may: loot
                    // lands where its corpse fell, and a corpse can fall
                    // against one. Faction is what tells them apart.
                    if (!world.tiles.IsWalkable(x, y) && world.registry.Has<FactionComponent>(occupant))
                    {
                        Fail(tick, "a creature is standing on unwalkable terrain");
                    }
                }
            }
        }

        // 2. Reverse: every entity that has a position is listed at that
        //    position. Catches the opposite drift -- something that fell
        //    out of the index and is now invisible to every spatial query,
        //    unfindable by AI and unpickable off the floor.
        //
        //    Counted as well as checked: equal totals is what rules out a
        //    duplicate listing, which the per-entity check alone cannot
        //    see.
        std::size_t positionedEntities = 0;
        world.registry.view<GridPositionComponent>().Each(
            [&](Entity entity, GridPositionComponent& position)
            {
                ++positionedEntities;
                if (!world.tiles.Contains(entity, position.x, position.y))
                {
                    Fail(tick, "an entity is missing from the index at its own tile");
                }
            });

        if (listedEntries != positionedEntities)
        {
            Fail(tick, "the tile index and the positioned entities do not tally");
        }

        // 3. Nothing structural is left half-done. Deaths resolve at the
        //    barrier, so no corpse survives a completed tick, and every
        //    attack request is consumed by the system that reads it.
        if (world.registry.Count<DeadComponent>() != 0)
        {
            Fail(tick, "a corpse survived the barrier");
        }
        if (world.registry.Count<AttackRequestComponent>() != 0)
        {
            Fail(tick, "an attack request went unconsumed");
        }
        if (world.registry.Count<PickupItemRequestComponent>() != 0)
        {
            Fail(tick, "a pickup request went unconsumed");
        }

        // 4. No negative health reached anything downstream.
        world.registry.view<HealthComponent>().Each(
            [&](Entity, HealthComponent& health)
            {
                if (health.current < 0)
                {
                    Fail(tick, "negative health");
                }
            });

        // 5. Spawners never overfill.
        //
        // Note what is deliberately NOT asserted: that aliveCount equals
        // the monsters that currently exist. It does not, and should not.
        // The count is recomputed at the top of stage 2, and the tick then
        // goes on to kill things (removed at the barrier) and spawn things
        // (created at the barrier), so by the time this audit runs it is a
        // snapshot from earlier in the same tick. It converges on the next
        // recount, which is the whole design -- a derived count that
        // repairs itself rather than one maintained by hand.
        //
        // The claim that does hold at every instant is the ceiling: no
        // spawner has more than it asked for, and the population never
        // exceeds the sum of what every spawner asked for.
        std::size_t linkedMonsters = 0;
        world.registry.view<SpawnedByComponent>().Each([&](Entity, SpawnedByComponent&) { ++linkedMonsters; });

        std::size_t totalDesired = 0;
        world.registry.view<SpawnerComponent>().Each(
            [&](Entity, SpawnerComponent& definition)
            {
                if (definition.aliveCount > definition.desiredCount)
                {
                    Fail(tick, "spawner reports more alive than it wants");
                }
                totalDesired += static_cast<std::size_t>(definition.desiredCount);
            });

        if (linkedMonsters > totalDesired)
        {
            Fail(tick, "more monsters exist than every spawner combined asked for");
        }

        // 6. Items: passable, well-formed, conserved, and announced. The
        //    equality itself is stated where it is checked, below.
        std::uint64_t onGround = 0;
        std::size_t groundItems = 0;
        world.registry.view<GroundItemComponent>().Each(
            [&](Entity item, GroundItemComponent& ground)
            {
                ++groundItems;
                onGround += ground.quantity;

                // A claimed item is despawned at the barrier, so none may
                // still be lying about with nothing left on it.
                if (ground.quantity == 0)
                {
                    Fail(tick, "a claimed ground item outlived the barrier");
                }

                // Items are on the index like everything else, and are kept
                // off the target list by having no faction rather than by
                // being hidden from the scan. Worth checking both halves:
                // an item that acquired a faction would become something
                // monsters queue up to kill.
                const GridPositionComponent* position = world.registry.TryGet<GridPositionComponent>(item);
                if (position == nullptr)
                {
                    Fail(tick, "a ground item has no position");
                }
                else if (!world.tiles.Contains(item, position->x, position->y))
                {
                    Fail(tick, "a ground item is missing from its own tile");
                }

                if (world.registry.Has<FactionComponent>(item))
                {
                    Fail(tick, "a ground item has a faction and is therefore a target");
                }
            });

        std::uint64_t carried = 0;
        std::size_t players = 0;
        world.registry.view<ViewerComponent>().Each(
            [&](Entity player, ViewerComponent&)
            {
                ++players;

                const InventoryComponent* inventory = world.registry.TryGet<InventoryComponent>(player);
                if (inventory == nullptr)
                {
                    return;
                }

                for (const InventoryComponent::Slot& slot : inventory->slots)
                {
                    carried += slot.quantity;

                    // Slot hygiene, which is Inventory.h's stated contract
                    // and the thing RemoveItem's clearing step exists to
                    // keep. A slot holding a quantity of item zero, or an
                    // item id with nothing left of it, is a half-cleared
                    // slot -- and slot indices go on the wire, so it would
                    // reach a client as a phantom stack.
                    if (slot.quantity == 0 && slot.itemId != 0)
                    {
                        Fail(tick, "an emptied slot kept its item id");
                    }
                    if (slot.quantity != 0 && slot.itemId == 0)
                    {
                        Fail(tick, "a slot holds a quantity of nothing");
                    }
                    if (slot.quantity > kLootMaxStack)
                    {
                        Fail(tick, "a stack grew past its limit");
                    }
                }
            });

        // Conservation.
        //
        // Everything ever *created* is either still on the ground, in
        // somebody's bag, or went into the grave with whoever was carrying
        // it -- exactly, with nothing over. Dropping does not appear on
        // either side: it moves goods from a bag to the floor and creates
        // nothing, which is precisely why it is worth running through this
        // check rather than around it.
        //
        // This is the sharpest tool here. A duplication bug shows up as a
        // surplus and a loss bug as a shortfall, both immediately and both
        // with a tick number, where either could otherwise run for hours
        // before anyone noticed the economy was wrong.
        if (onGround + carried + m_itemsLost + m_itemsExpired != m_itemsCreated)
        {
            Fail(tick, "items are not conserved");
        }

        // And every item that appeared on the ground was announced exactly
        // once, whether it came from a corpse or a player's bag. Catches an
        // item that arrives without an event -- invisible to every client,
        // and to the check above.
        if (m_itemsAnnounced != m_itemsCreated + m_itemsRecirculated)
        {
            Fail(tick, "a ground item appeared without being announced");
        }

        // A pickup refused for want of an inventory is a wiring mistake,
        // not a gameplay outcome -- ItemEvents.h keeps the reason distinct
        // precisely so it cannot hide as a full bag. Nothing in this
        // scenario picks up without one, so any occurrence means the
        // scenario has drifted from what it thinks it built.
        if (m_pickupFailures[static_cast<std::size_t>(PickupFailure::NoInventory)] != 0)
        {
            Fail(tick, "a picker had no inventory");
        }

        // Targets survive slot recycling.
        //
        // AIComponent::currentTarget is an Entity rather than a bare id so
        // that a target which died and had its slot reused fails
        // Registry::Exists instead of silently retargeting whatever
        // inherited it. A stale handle is therefore expected here and not
        // checked -- a target killed this tick is despawned at the barrier,
        // and the AI clears the handle on its next pass.
        //
        // What must never happen is a stale handle that *resolves*. If the
        // generation check were broken, a recycled slot would come back
        // alive, and the entity found there would be an arbitrary one: a
        // ground item with no faction, another monster on the same side, or
        // the monster itself. Each of those is caught below.
        world.registry.view<AIComponent, FactionComponent>().Each(
            [&](Entity self, AIComponent& ai, FactionComponent& faction)
            {
                if (ai.currentTarget == kNullEntity || !world.registry.Exists(ai.currentTarget))
                {
                    return;
                }

                if (ai.currentTarget == self)
                {
                    Fail(tick, "an AI is targeting itself");
                    return;
                }

                const FactionComponent* targetFaction =
                    world.registry.TryGet<FactionComponent>(ai.currentTarget);
                if (targetFaction == nullptr)
                {
                    Fail(tick, "an AI is targeting something with no faction");
                }
                else if (targetFaction->faction == faction.faction)
                {
                    Fail(tick, "an AI is targeting its own side");
                }
            });

        CheckNoticesReachedTheRightViewers(tick);

        // 7. Every entity belongs to exactly one category the scenario
        //    knows about. A stray -- something created and never reclaimed,
        //    or reclaimed twice -- shows up as a mismatch rather than as
        //    slow growth nobody is watching.
        std::size_t spawners = 0;
        world.registry.view<SpawnerComponent>().Each([&](Entity, SpawnerComponent&) { ++spawners; });

        if (players + spawners + linkedMonsters + groundItems != world.registry.AliveCount())
        {
            Fail(tick, "an entity belongs to no category the scenario accounts for");
        }
    }

    // Broadcast, checked in both directions.
    //
    // Counting notices -- which the load test also does -- proves only that
    // the stream is not empty. A BroadcastSystem that sent every notice to
    // every viewer would sail through a count check while flooding clients
    // with things happening across the map, and one that filtered too
    // hard would leave monsters dying invisibly a few tiles away. Both are
    // silent, and both are the kind of bug that only shows up as a player
    // complaint months later.
    //
    // So: nothing out of range arrived, and nothing in range was missed.
    void CheckNoticesReachedTheRightViewers(int tick)
    {
        MapWorld& world = m_simulation.World();

        // Soundness. Note this re-derives the same rule BroadcastSystem
        // applies, so it does not police the rule itself -- what it catches
        // is the delivery plumbing around it: a notice handed to the wrong
        // viewer, a stale position used for filtering, a radius read off
        // the wrong component.
        for (const DeliveredNotice& delivered : m_tickNotices)
        {
            if (delivered.notice.subject == delivered.viewer)
            {
                // A viewer always hears about itself, at any distance.
                continue;
            }

            const ViewerComponent* viewer = world.registry.TryGet<ViewerComponent>(delivered.viewer);
            const GridPositionComponent* position =
                world.registry.TryGet<GridPositionComponent>(delivered.viewer);
            if (viewer == nullptr || position == nullptr)
            {
                Fail(tick, "a notice went to something that is not a viewer on the map");
                continue;
            }

            const bool nearDestination =
                Chebyshev(position->x, position->y, delivered.notice.x, delivered.notice.y) <= viewer->radius;
            const bool nearOrigin =
                delivered.notice.kind == NoticeKind::Moved &&
                Chebyshev(position->x, position->y, delivered.notice.fromX, delivered.notice.fromY) <=
                    viewer->radius;

            if (!nearDestination && !nearOrigin)
            {
                Fail(tick, "a viewer was told about something it cannot see");
            }
        }

        // Completeness, for the two kinds most likely to go missing.
        //
        // Both announce something that is *gone or new* rather than
        // something continuing, so both are easy to drop silently: the
        // subject of a death is despawned in the same barrier that
        // announces it, and an item appearing was for a long time not
        // broadcast at all -- the client was told loot vanished that it
        // was never told existed. A missed death leaves a corpse standing
        // on a client forever; a missed appearance leaves loot invisible
        // and unclickable.
        if (m_tickDeaths.empty() && m_tickItemSpawns.empty())
        {
            return;
        }

        world.registry.view<ViewerComponent, GridPositionComponent>().Each(
            [&](Entity viewer, ViewerComponent& viewerComponent, GridPositionComponent& position)
            {
                for (const RecordedDeath& death : m_tickDeaths)
                {
                    if (Chebyshev(position.x, position.y, death.x, death.y) > viewerComponent.radius)
                    {
                        continue;
                    }

                    if (!WasTold(viewer, NoticeKind::Died, death.entity))
                    {
                        Fail(tick, "a death in plain sight was never announced to a viewer");
                    }
                }

                for (const RecordedAppearance& appearance : m_tickItemSpawns)
                {
                    if (Chebyshev(position.x, position.y, appearance.x, appearance.y) > viewerComponent.radius)
                    {
                        continue;
                    }

                    if (!WasTold(viewer, NoticeKind::ItemAppeared, appearance.entity))
                    {
                        Fail(tick, "loot dropped in plain sight was never announced to a viewer");
                    }
                }
            });
    }

    bool WasTold(Entity viewer, NoticeKind kind, Entity subject) const
    {
        for (const DeliveredNotice& delivered : m_tickNotices)
        {
            if (delivered.viewer == viewer && delivered.notice.kind == kind && delivered.notice.subject == subject)
            {
                return true;
            }
        }
        return false;
    }

    static int Chebyshev(int ax, int ay, int bx, int by)
    {
        const int dx = ax > bx ? ax - bx : bx - ax;
        const int dy = ay > by ? ay - by : by - ay;
        return dx > dy ? dx : dy;
    }

    void Digest()
    {
        MapWorld& world = m_simulation.World();

        Mix(world.registry.AliveCount());
        Mix(m_deaths);
        Mix(m_spawns);
        Mix(m_itemsCreated);
        Mix(m_itemsRecirculated);
        Mix(m_itemsExpired);
        Mix(m_pickups);

        // Walked in tile order rather than entity order, so the digest does
        // not depend on internal pool layout -- only on where things
        // actually are.
        for (int y = 0; y < kMapSize; ++y)
        {
            for (int x = 0; x < kMapSize; ++x)
            {
                // Every occupant of every tile, in arrival order. Folding
                // the whole list in rather than one handle per tile is what
                // keeps the digest sensitive to a crowd rearranging itself
                // on one square -- which, now that crowds can, is a state
                // two runs have to agree on.
                for (const Entity occupant : world.tiles.OccupantsAt(x, y))
                {
                    Mix(static_cast<std::uint64_t>(y) * kMapSize + x);
                    const HealthComponent* health = world.registry.TryGet<HealthComponent>(occupant);
                    Mix(health != nullptr ? static_cast<std::uint64_t>(health->current) : 0u);
                }
            }
        }
    }

    void Mix(std::uint64_t value)
    {
        m_digest ^= value;
        m_digest *= 1099511628211ull;
    }

    void Fail(int tick, const char* what)
    {
        if (m_failures < 8)
        {
            std::printf("  FAIL  tick %d: %s\n", tick, what);
        }
        ++m_failures;
        ++world_v2::test::g_failures;
    }

    // What reached one viewer this tick, kept only until the audit has
    // looked at it.
    struct DeliveredNotice
    {
        Entity viewer = kNullEntity;
        Notice notice;
    };

    struct RecordedDeath
    {
        Entity entity = kNullEntity;
        int x = 0;
        int y = 0;
    };

    // Same shape, for loot hitting the floor. Separate because the notice
    // kind differs, and completeness is checked per kind.
    struct RecordedAppearance
    {
        Entity entity = kNullEntity;
        int x = 0;
        int y = 0;
    };

    Config m_config;
    Simulation m_simulation;
    std::vector<Entity> m_players;
    std::vector<Entity> m_spawners;
    Entity m_walledInSpawner = kNullEntity;

    std::vector<DeliveredNotice> m_tickNotices;
    std::vector<RecordedDeath> m_tickDeaths;
    std::vector<RecordedAppearance> m_tickItemSpawns;

    std::uint64_t m_digest = 14695981039346656037ull;
    std::size_t m_notices = 0;
    std::size_t m_deaths = 0;
    std::size_t m_spawns = 0;
    std::size_t m_lootDrops = 0;
    std::size_t m_levelUps = 0;
    std::size_t m_pickups = 0;
    std::size_t m_drops = 0;
    std::size_t m_peakTileOccupancy = 0;

    // Indexed by PickupFailure. Sized by hand because the enum has no count
    // member and inventing one to serve a test would be the tail wagging
    // the dog; the check below fails loudly if a reason is ever added.
    std::size_t m_pickupFailures[4] = {0, 0, 0, 0};

    // Items that came into the world from nothing (loot), as against those
    // that merely changed hands (drops). Only the first belongs in the
    // conservation equation; both belong in the announcement count.
    std::uint64_t m_itemsCreated = 0;
    std::uint64_t m_itemsRecirculated = 0;
    std::uint64_t m_itemsAnnounced = 0;
    std::uint64_t m_itemsLost = 0;
    std::uint64_t m_itemsExpired = 0;
    std::size_t m_lootTimeouts = 0;
    int m_failures = 0;
};

// Guards the hand-sized array above: if a PickupFailure reason is ever
// added, this stops compiling instead of silently going uncounted.
static_assert(static_cast<std::size_t>(PickupFailure::InventoryFull) == 3,
              "PickupFailure gained a reason -- widen Scenario::m_pickupFailures");

// A numbering of its own, so these ids start from zero no matter what the
// rest of the file has touched.
struct ColdFamily
{
};

template <int N>
struct ColdTag
{
};

// One probe per cold type, as function pointers, so a runtime loop can
// reach compile-time-distinct types.
template <int... Ns>
std::vector<TypeId (*)()> MakeColdProbes(std::integer_sequence<int, Ns...>)
{
    return {(+[]() { return TypeIdOf<ColdFamily>::Value<ColdTag<Ns>>(); })...};
}

void ColdTypeIdsAreHandedOutUniquely()
{
    // Races the type-id counter the way a server bringing up several map
    // threads at once does: many types, none of them touched before, all
    // first touched at the same instant.
    //
    // The function-local static inside Value<T>() serializes the
    // initialization of *one* type's id. It says nothing about two
    // different types initializing concurrently -- those are two unrelated
    // statics whose initializers both run, and both land on the shared
    // counter inside Next(). A non-atomic increment there can hand two
    // component types the same id, and in Registry the id *is* the pool
    // index, so two types would silently share storage.
    //
    // Must run first. Once a type's id exists, asking again is a plain read
    // and there is nothing left to race.
    //
    // Honest about what this is: a stressor, not a proof. A race can be
    // lost a thousand times and won on the thousand-and-first. The starting
    // gate below is what makes the threads collide reliably enough to be
    // worth running; the atomic in TypeId.h is what makes it correct.
    constexpr int kColdTypes = 64;
    constexpr int kThreads = 8;

    const std::vector<TypeId (*)()> probes = MakeColdProbes(std::make_integer_sequence<int, kColdTypes>{});
    std::vector<TypeId> ids(kColdTypes, 0);

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back(
            [&probes, &ids, &ready, &go, t]()
            {
                ready.fetch_add(1);
                while (!go.load())
                {
                    std::this_thread::yield();
                }

                // Interleaved rather than blocked, so neighbouring types
                // are claimed by different threads.
                for (int i = t; i < kColdTypes; i += kThreads)
                {
                    ids[static_cast<std::size_t>(i)] = probes[static_cast<std::size_t>(i)]();
                }
            });
    }

    while (ready.load() < kThreads)
    {
        std::this_thread::yield();
    }
    go.store(true);

    for (std::thread& thread : threads)
    {
        thread.join();
    }

    // Dense from zero and all distinct -- exactly what Registry and
    // EventManager assume when they use an id as a vector index. Sorting
    // and comparing against the sequence checks both properties at once: a
    // duplicate necessarily leaves a hole.
    std::vector<TypeId> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    for (int i = 0; i < kColdTypes; ++i)
    {
        CHECK_EQ(sorted[static_cast<std::size_t>(i)], static_cast<TypeId>(i));
    }
}

void TheWorldStaysConsistentUnderLoad()
{
    Scenario scenario;

    for (int tick = 0; tick < 300; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);

    // The run has to have actually exercised something. A scenario that
    // quietly did nothing would satisfy every invariant above.
    CHECK(scenario.Deaths() > 20);
    CHECK(scenario.Spawns() > static_cast<std::size_t>(kSpawnerCount * kMonstersPerSpawner));
    CHECK(scenario.Notices() > 1000);
    CHECK(scenario.LootDrops() > 20);
    CHECK(scenario.LevelUps() > 0);
    CHECK(scenario.Pickups() > 5);

    // Entities really did share tiles. Without this the run could have
    // stayed accidentally one-per-tile -- monsters converging on a player
    // and loot falling under their feet both produce stacks -- and every
    // shared-tile invariant above would have been checking nothing.
    CHECK(scenario.PeakTileOccupancy() > 1);

    std::printf("  ..    %zu deaths, %zu spawns, %zu loot drops, %zu pickups, %zu level-ups, %zu notices, "
                "%zu deep at the busiest tile\n",
                scenario.Deaths(), scenario.Spawns(), scenario.LootDrops(), scenario.Pickups(), scenario.LevelUps(),
                scenario.Notices(), scenario.PeakTileOccupancy());
}

void ItemsChangeHandsWithoutMultiplying()
{
    // The full item loop, both directions: corpses drop loot, players pick
    // it up, players put things back down, and other players pick those up
    // again. Hundreds of transfers, and the conservation check audits the
    // total after every single one.
    //
    // The direction that does not exist in the framework is the one going
    // out of a bag -- see the note at the top of this file. What is being
    // tested here is therefore two things at once: that the item accounting
    // survives churn, and that the seam is sufficient for a game layer to
    // implement dropping on top of, since today it has to.
    Config config;
    config.dropping = true;

    Scenario scenario(config);

    for (int tick = 0; tick < 300; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);

    // Both halves have to have actually happened, or conservation held
    // over a loop that never ran.
    CHECK(scenario.Drops() > 20);
    CHECK(scenario.Pickups() > 20);

    // And the goods really did come back off the floor: more pickups than
    // corpses could account for on their own means dropped stacks were
    // being retrieved.
    CHECK(scenario.Pickups() > scenario.LootDrops() / 2);

    std::printf("  ..    %zu pickups, %zu drops, %zu refused (bag full %zu, gone %zu)\n", scenario.Pickups(),
                scenario.Drops(),
                scenario.PickupsRefused(PickupFailure::InventoryFull) +
                    scenario.PickupsRefused(PickupFailure::Gone) +
                    scenario.PickupsRefused(PickupFailure::OutOfRange),
                scenario.PickupsRefused(PickupFailure::InventoryFull),
                scenario.PickupsRefused(PickupFailure::Gone));
}

// Wires the pickup path onto a bare Simulation. The small item tests below
// want a world with two things in it, not a map with six spawner camps.
void WirePickupOnly(Simulation& simulation)
{
    InstallItemRules(simulation.World());

    simulation.Commands().On<PickupCommand>(
        [](MapWorld& world, const PickupCommand& command)
        {
            if (!world.registry.Exists(command.picker) || !world.registry.Exists(command.item))
            {
                return;
            }
            world.registry.Assign<PickupItemRequestComponent>(command.picker, command.item);
        });
}

void TwoPlayersRacingForOneItemGetOneCopy()
{
    // The duplication guard, driven through a whole tick rather than
    // through PickupSystem alone.
    //
    // This is the case that happens every time something good drops, and
    // the reason it is dangerous is timing: the item entity is not removed
    // until the barrier, so "does it still exist" is true for both pickers
    // when they are resolved in the same sweep. The guard is that taking an
    // item zeroes its quantity immediately. Going through the command queue
    // adds what the unit test cannot: both requests arriving as separate
    // inbound commands, drained together, resolved in one sweep, and the
    // husk cleaned up by ItemRules at the barrier.
    Simulation simulation(16, 16, true);
    WirePickupOnly(simulation);

    std::size_t taken = 0;
    std::size_t refused = 0;
    PickupFailure refusedReason = PickupFailure::NoInventory;

    simulation.World().events.Listen<ItemPickedUpEvent>([&taken](const ItemPickedUpEvent&) { ++taken; });
    simulation.World().events.Listen<ItemPickupFailedEvent>(
        [&refused, &refusedReason](const ItemPickupFailedEvent& event)
        {
            ++refused;
            refusedReason = event.reason;
        });

    const Entity first = simulation.World().Spawn(4, 4);
    const Entity second = simulation.World().Spawn(6, 4);
    CHECK(first != kNullEntity);
    CHECK(second != kNullEntity);

    simulation.World().registry.Assign<InventoryComponent>(first, MakeInventory(4));
    simulation.World().registry.Assign<InventoryComponent>(second, MakeInventory(4));

    // Between them, one tile from each.
    const Entity item = SpawnGroundItem(simulation.World(), 77, 1, kLootMaxStack, 5, 4);
    CHECK(item != kNullEntity);

    simulation.Commands().Push(PickupCommand{first, item});
    simulation.Commands().Push(PickupCommand{second, item});
    simulation.Tick(kTickSeconds);

    CHECK_EQ(taken, std::size_t{1});
    CHECK_EQ(refused, std::size_t{1});
    CHECK(refusedReason == PickupFailure::Gone);

    // The husk was cleaned up at the barrier, in the same tick.
    CHECK(!simulation.World().registry.Exists(item));

    // And exactly one copy exists in the world. This is the assertion that
    // would catch the duplication the guard exists to prevent.
    const std::uint32_t firstHolds =
        CountItem(simulation.World().registry.Get<InventoryComponent>(first), 77);
    const std::uint32_t secondHolds =
        CountItem(simulation.World().registry.Get<InventoryComponent>(second), 77);
    CHECK_EQ(firstHolds + secondHolds, std::uint32_t{1});
}

void AFullBagLeavesTheLootWhereItFell()
{
    // A refused pickup must be a no-op on both sides. The failure mode
    // worth guarding is the one where the item is claimed -- quantity
    // zeroed, entity despawned at the barrier -- by a transfer that then
    // did not happen, which destroys the goods outright.
    //
    // PickupSystem is written to make that impossible by ordering: every
    // step that can fail happens before the claim. This is the end-to-end
    // check that the ordering survived.
    Simulation simulation(16, 16, true);
    WirePickupOnly(simulation);

    std::size_t refused = 0;
    PickupFailure refusedReason = PickupFailure::Gone;
    simulation.World().events.Listen<ItemPickupFailedEvent>(
        [&refused, &refusedReason](const ItemPickupFailedEvent& event)
        {
            ++refused;
            refusedReason = event.reason;
        });

    const Entity player = simulation.World().Spawn(4, 4);
    CHECK(player != kNullEntity);

    // One slot, filled to the brim with something else.
    InventoryComponent bag = MakeInventory(1);
    CHECK(TryAddItem(bag, 11, kLootMaxStack, kLootMaxStack));
    simulation.World().registry.Assign<InventoryComponent>(player, bag);

    const Entity item = SpawnGroundItem(simulation.World(), 22, 1, kLootMaxStack, 4, 4);
    CHECK(item != kNullEntity);

    simulation.Commands().Push(PickupCommand{player, item});
    simulation.Tick(kTickSeconds);

    CHECK_EQ(refused, std::size_t{1});
    CHECK(refusedReason == PickupFailure::InventoryFull);

    // Still lying there, whole.
    CHECK(simulation.World().registry.Exists(item));
    const GroundItemComponent& ground = simulation.World().registry.Get<GroundItemComponent>(item);
    CHECK_EQ(ground.itemId, std::uint32_t{22});
    CHECK_EQ(ground.quantity, std::uint32_t{1});

    // And the bag is exactly as it was.
    const InventoryComponent& held = simulation.World().registry.Get<InventoryComponent>(player);
    CHECK_EQ(CountItem(held, 11), kLootMaxStack);
    CHECK_EQ(CountItem(held, 22), std::uint32_t{0});
}

void TwoIdenticalRunsAgreeExactly()
{
    // Nothing in the simulation is random, so the same inputs must produce
    // the same world and the same outbound stream -- not merely a similar
    // one. Without this, a bug report is not reproducible and a replay is
    // not possible.
    std::uint64_t first = 0;
    std::size_t firstDeaths = 0;
    std::size_t firstNotices = 0;

    for (int run = 0; run < 2; ++run)
    {
        Scenario scenario;
        for (int tick = 0; tick < 150; ++tick)
        {
            scenario.Step(tick);
        }

        CHECK_EQ(scenario.Failures(), 0);

        if (run == 0)
        {
            first = scenario.DigestValue();
            firstDeaths = scenario.Deaths();
            firstNotices = scenario.Notices();
        }
        else
        {
            CHECK_EQ(scenario.DigestValue(), first);
            CHECK_EQ(scenario.Deaths(), firstDeaths);
            CHECK_EQ(scenario.Notices(), firstNotices);
        }
    }
}

void APlayerDyingMidRunIsHandledCleanly()
{
    Scenario scenario;

    for (int tick = 0; tick < 40; ++tick)
    {
        scenario.Step(tick);
    }

    // Kill one outright, then keep running. Its queued commands, the
    // monsters targeting it, and the tile it was standing on all have to
    // resolve without anything drifting.
    const Entity victim = scenario.Players().front();
    CHECK(scenario.Sim().World().registry.Exists(victim));

    const GridPositionComponent& position = scenario.Sim().World().registry.Get<GridPositionComponent>(victim);
    const int deathX = position.x;
    const int deathY = position.y;

    scenario.Sim().World().registry.Get<HealthComponent>(victim).current = 0;

    for (int tick = 40; tick < 140; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);
    CHECK(!scenario.Sim().World().registry.Exists(victim));

    // It came off the index rather than lingering as a phantom the AI can
    // still see. (Others may well be standing there by now -- what matters
    // is that the dead player is not among them.)
    CHECK(!scenario.Sim().World().tiles.Contains(victim, deathX, deathY));
}

void MapsTickInParallelWithoutInterference()
{
    // The claim MapWorld and Simulation both make, and which nothing tested
    // before: maps share no mutable state, so several can tick on separate
    // threads at once provided no single one is ticked twice at once.
    //
    // Checking it by digest rather than by "it did not crash" is what gives
    // it teeth. Each thread runs the identical scenario, so a map whose
    // state was touched by another thread -- a shared pool, a type id
    // collision, a static hiding in a system -- comes back with a different
    // digest, even if nothing crashed and every per-tick invariant held.
    // A digest that matches the single-threaded reference means the map
    // genuinely ran alone.
    constexpr int kTicks = 150;
    constexpr int kMaps = 4;

    std::uint64_t reference = 0;
    {
        Scenario scenario;
        for (int tick = 0; tick < kTicks; ++tick)
        {
            scenario.Step(tick);
        }

        CHECK_EQ(scenario.Failures(), 0);
        reference = scenario.DigestValue();
    }

    std::vector<std::uint64_t> digests(kMaps, 0);
    std::vector<int> failures(kMaps, 0);

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    std::vector<std::thread> maps;
    maps.reserve(kMaps);
    for (int i = 0; i < kMaps; ++i)
    {
        maps.emplace_back(
            [&digests, &failures, &ready, &go, i]()
            {
                // Built before the gate so construction is not what
                // overlaps -- the ticking is.
                Scenario scenario;

                ready.fetch_add(1);
                while (!go.load())
                {
                    std::this_thread::yield();
                }

                for (int tick = 0; tick < kTicks; ++tick)
                {
                    scenario.Step(tick);
                }

                digests[static_cast<std::size_t>(i)] = scenario.DigestValue();
                failures[static_cast<std::size_t>(i)] = scenario.Failures();
            });
    }

    while (ready.load() < kMaps)
    {
        std::this_thread::yield();
    }
    go.store(true);

    for (std::thread& map : maps)
    {
        map.join();
    }

    for (int i = 0; i < kMaps; ++i)
    {
        CHECK_EQ(failures[static_cast<std::size_t>(i)], 0);
        CHECK_EQ(digests[static_cast<std::size_t>(i)], reference);
    }
}

void ALevelCurveAdvancesWithoutSticking()
{
    // The game layer's half of the level-up cascade, which every other test
    // here deliberately leaves out.
    //
    // CombatRules raises the level and stops; what the next level costs is
    // level.scr data it will not invent. With no listener supplying one,
    // requiredForNextLevel never moves, every subsequent award re-levels
    // immediately, and kMaxLevelsPerAward -- documented as a guard against
    // a mistake -- quietly becomes the thing holding the loop together.
    //
    // With a real curve installed, the observable is the residue: a player
    // must never be sitting on more experience than its next level costs.
    // That state can only arise if an award stopped early, which is to say
    // if the cap was reached.
    Config config;
    config.levelCurve = true;

    Scenario scenario(config);

    for (int tick = 0; tick < 300; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);
    CHECK(scenario.LevelUps() > 0);

    std::size_t levelled = 0;
    for (const Entity player : scenario.Players())
    {
        if (!scenario.Sim().World().registry.Exists(player))
        {
            continue;
        }

        const ExperienceComponent& experience =
            scenario.Sim().World().registry.Get<ExperienceComponent>(player);

        // A threshold of zero would mean CombatRules stops levelling
        // entirely -- the AZeroThresholdCannotSpin case -- which is not
        // what a curve should ever produce.
        CHECK(experience.requiredForNextLevel > 0);
        CHECK(experience.current < experience.requiredForNextLevel);

        if (experience.level > 1)
        {
            ++levelled;
        }
    }

    CHECK(levelled > 0);
}

void AWalledInSpawnerNeverPlacesOrBreaks()
{
    // A camp sealed in solid rock, asking forever and never succeeding.
    //
    // SpawnRules drops a request whose FindSpawnTile comes back empty and
    // remembers nothing, on the reasoning that SpawnSystem recounts from
    // scratch next tick and will ask again. That is a loop with no exit
    // condition, so the things worth pinning down are that it stays
    // harmless: no phantom monsters counted, no sequence advanced on a
    // placement that never happened, no effect on the spawners that can
    // place, and no wedged tick.
    Config config;
    config.walledInSpawner = true;

    Scenario scenario(config);

    for (int tick = 0; tick < 200; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);

    const SpawnerComponent& sealed =
        scenario.Sim().World().registry.Get<SpawnerComponent>(scenario.WalledInSpawner());

    CHECK_EQ(sealed.aliveCount, 0);

    // The sequence is what makes successive monsters from one spawner land
    // on different tiles. Advancing it on a request that placed nothing
    // would silently skew every future placement from this spawner.
    CHECK_EQ(sealed.sequence, std::uint32_t{0});

    // And the healthy camps carried on regardless.
    CHECK(scenario.Spawns() > static_cast<std::size_t>(kSpawnerCount * kMonstersPerSpawner));
}

void CommandsNamingDeadEntitiesAreRejected()
{
    // The window CommandQueue documents: a handle is pushed on a connection
    // thread and the entity it names dies before the drain reaches it.
    //
    // Players here are frail and are ordered to attack each other, so they
    // genuinely die mid-run while commands naming them keep arriving. The
    // generation packed into an Entity is what has to turn each of those
    // into a rejected command rather than a write to whoever inherited the
    // slot -- and slots are being recycled briskly by monster spawns, so
    // there is something waiting to inherit them.
    Config config;
    config.playerHealth = 30;

    Scenario scenario(config);
    const std::vector<Entity> originals = scenario.Players();
    CHECK(!originals.empty());

    for (int tick = 0; tick < 250; ++tick)
    {
        // Every original, alive or not, every tick. Note the pickup command
        // names another *player* rather than an item: a handle that is not
        // only stale but was never the right kind of thing, which is what a
        // malformed or replayed packet looks like.
        for (const Entity player : originals)
        {
            scenario.Sim().Commands().Push(MoveCommand{player, 1, 0});
            scenario.Sim().Commands().Push(AttackCommand{player, originals.front(), 3});
            scenario.Sim().Commands().Push(PickupCommand{player, originals.back()});
        }

        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);

    // Every command found a handler; none was silently dropped for want of
    // one.
    CHECK_EQ(scenario.Sim().Commands().UnhandledCount(), std::size_t{0});

    // The test proves nothing if they all survived.
    std::size_t gone = 0;
    for (const Entity player : originals)
    {
        if (!scenario.Sim().World().registry.Exists(player))
        {
            ++gone;
        }
    }
    CHECK(gone > 0);

    std::printf("  ..    %zu of %zu players died under friendly fire\n", gone, originals.size());
}

void LootThatNobodyCollectsStopsPilingUp()
{
    // The case despawn timers exist for, and the one the soak below cannot
    // assert without them.
    //
    // Loot has no natural ceiling: nothing sweeps the floor, and an item
    // that falls where no player walks lies there for as long as the
    // server runs. At worst case -- a zone with more drops than foot
    // traffic -- that is unbounded growth, and it is not hypothetical,
    // because the AI vision scan walks every entity on every tile it
    // looks at. A floor deep in loot is a floor that costs more to think
    // about, measurably: the benchmark puts sixteen items per tile at
    // twenty-four times the scan cost of a bare one.
    //
    // Players ignore loot here on purpose, so every drop stays where it
    // fell and the timer is the only thing that removes anything. They
    // still fight normally, which is what keeps the drops coming.
    Config config;
    config.lootLifetimeSeconds = 4.0f;
    config.collectLoot = false;

    Scenario scenario(config);

    std::size_t peakGroundItems = 0;
    for (int tick = 0; tick < 600; ++tick)
    {
        scenario.Step(tick);

        std::size_t groundItems = 0;
        scenario.Sim().World().registry.view<GroundItemComponent>().Each(
            [&groundItems](Entity, GroundItemComponent&) { ++groundItems; });

        if (groundItems > peakGroundItems)
        {
            peakGroundItems = groundItems;
        }
    }

    CHECK_EQ(scenario.Failures(), 0);

    // Timers actually fired, and kept firing.
    CHECK(scenario.LootTimeouts() > 20);

    // The ceiling is what the timer buys: at a four-second lifetime and a
    // quarter-second tick nothing survives past sixteen ticks, so the
    // floor only ever holds what the last sixteen ticks dropped.
    //
    // Tight on purpose. A generous bound here would be worthless: this run
    // produces about sixty items in total, so any ceiling above that passes
    // whether or not a single timer ever fires. Sixteen is comfortably
    // above the four this actually reaches and far below the sixty it
    // would reach with the timers off -- verified by turning them off.
    CHECK(peakGroundItems < 16);

    // With nobody able to carry anything, every item that left did so
    // through the timer. Conservation above already proved none were
    // duplicated; this proves the exit route was the intended one.
    CHECK_EQ(scenario.Pickups(), std::size_t{0});

    std::printf("  ..    %zu loot timeouts, %zu on the floor at the worst moment\n", scenario.LootTimeouts(),
                peakGroundItems);
}

void ALongRunDoesNotDrift()
{
    // A soak. Every invariant above, twelve hundred ticks deep.
    //
    // The failures this catches are the ones that need time: a leak of one
    // entity per tick, a counter that creeps, a pool that grows and never
    // shrinks. None of them is visible in a three-hundred-tick run, and all
    // of them matter on a server that stays up for weeks.
    constexpr int kTicks = 1200;

    Scenario scenario;
    for (int tick = 0; tick < kTicks; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);

    std::size_t groundItems = 0;
    scenario.Sim().World().registry.view<GroundItemComponent>().Each(
        [&groundItems](Entity, GroundItemComponent&) { ++groundItems; });

    const std::size_t alive = scenario.Sim().World().registry.AliveCount();
    CHECK(alive >= groundItems);

    // Everything that is not loot is bounded by construction: the players,
    // the spawners, and every spawner's full complement. Nothing else may
    // accumulate.
    const std::size_t ceiling = static_cast<std::size_t>(kPlayerCount) + static_cast<std::size_t>(kSpawnerCount) +
                                static_cast<std::size_t>(kSpawnerCount) *
                                    static_cast<std::size_t>(kMonstersPerSpawner);
    CHECK((alive - groundItems) <= ceiling);

    // Loot is the exception, and deliberately not asserted against a
    // ceiling, because there is nothing in world_v2 that would enforce one:
    // GroundItemComponent has no lifetime, and nothing sweeps the floor. An
    // item that falls where no player goes lies there for as long as the
    // server runs. That is fine for a test and a slow leak on a live
    // server -- the number is printed rather than checked so that the
    // growth is at least visible.
    std::printf("  ..    %d ticks, %zu entities alive, %zu of them unclaimed loot\n", kTicks, alive, groundItems);
}

void CommandsFromManyThreadsSurviveALiveSimulation()
{
    // The seam under its real shape: connection threads pushing while the
    // simulation thread ticks. Determinism cannot hold here -- arrival
    // order is genuinely nondeterministic -- but nothing may be lost, and
    // every invariant still has to hold on every tick.
    Scenario scenario;

    // Bounded on purpose. An unthrottled push loop does not test the seam
    // any harder -- it just buries the simulation under millions of queued
    // commands, and the tick never finishes. A fixed budget per thread
    // keeps the contention while leaving the run finite.
    constexpr int kPushesPerThread = 400;
    constexpr int kSenderThreads = 4;

    std::atomic<int> pushed{0};

    std::vector<std::thread> senders;
    senders.reserve(kSenderThreads);
    for (int t = 0; t < kSenderThreads; ++t)
    {
        senders.emplace_back(
            [&scenario, &pushed, t]()
            {
                for (int i = 0; i < kPushesPerThread; ++i)
                {
                    const Entity player = scenario.Players()[static_cast<std::size_t>(t) % scenario.Players().size()];
                    const std::uint32_t roll = Hash(static_cast<std::uint32_t>(t), static_cast<std::uint32_t>(i));
                    const int direction = static_cast<int>(roll % 9);

                    scenario.Sim().Commands().Push(MoveCommand{player, (direction % 3) - 1, (direction / 3) - 1});
                    pushed.fetch_add(1);
                    std::this_thread::yield();
                }
            });
    }

    for (int tick = 0; tick < 150; ++tick)
    {
        scenario.Step(tick);
    }

    for (std::thread& sender : senders)
    {
        sender.join();
    }

    // Drain whatever landed after the last tick.
    scenario.Sim().Tick(kTickSeconds);

    CHECK_EQ(scenario.Failures(), 0);
    CHECK_EQ(pushed.load(), kSenderThreads * kPushesPerThread);
    CHECK_EQ(scenario.Sim().Commands().PendingCount(), 0u);
    CHECK_EQ(scenario.Sim().Commands().UnhandledCount(), 0u);
}

} // namespace

int main()
{
    // First, before anything else in this process has asked for a type id:
    // the race it checks only exists on first use.
    ColdTypeIdsAreHandedOutUniquely();

    TheWorldStaysConsistentUnderLoad();
    ItemsChangeHandsWithoutMultiplying();
    TwoPlayersRacingForOneItemGetOneCopy();
    AFullBagLeavesTheLootWhereItFell();
    TwoIdenticalRunsAgreeExactly();
    MapsTickInParallelWithoutInterference();
    ALevelCurveAdvancesWithoutSticking();
    AWalledInSpawnerNeverPlacesOrBreaks();
    APlayerDyingMidRunIsHandledCleanly();
    CommandsNamingDeadEntitiesAreRejected();
    CommandsFromManyThreadsSurviveALiveSimulation();
    LootThatNobodyCollectsStopsPilingUp();
    ALongRunDoesNotDrift();

    return world_v2::test::Summary("Scenario");
}
