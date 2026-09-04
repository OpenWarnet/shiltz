#include "CommandQueue.h"
#include "component/Grid.h"
#include "core/Entity.h"
#include "core/Test.h"
#include "world/MapWorld.h"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

using namespace world_v2;

namespace
{

struct MoveCommand
{
    Entity entity = kNullEntity;
    int directionX = 0;
    int directionY = 0;
};

struct SpawnCommand
{
    int x = 0;
    int y = 0;
};

struct UnwiredCommand
{
    int value = 0;
};

void PushDefersUntilDrain()
{
    MapWorld world(8, 8, true);
    CommandQueue commands;

    const Entity entity = world.Spawn(2, 2);

    commands.On<MoveCommand>(
        [](MapWorld& target, const MoveCommand& command)
        {
            if (!target.registry.Exists(command.entity))
            {
                return;
            }
            target.registry.Assign<MoveIntentComponent>(command.entity, command.directionX, command.directionY);
        });

    commands.Push(MoveCommand{entity, 1, 0});

    // Nothing touches the world until the simulation thread asks for it.
    CHECK_EQ(commands.PendingCount(), 1u);
    CHECK(!world.registry.Has<MoveIntentComponent>(entity));

    CHECK_EQ(commands.Drain(world), 1u);
    CHECK_EQ(commands.PendingCount(), 0u);
    CHECK(world.registry.Has<MoveIntentComponent>(entity));
    CHECK_EQ(world.registry.Get<MoveIntentComponent>(entity).directionX, 1);

    // Draining an empty queue is a no-op.
    CHECK_EQ(commands.Drain(world), 0u);
}

void CommandsApplyInPushOrder()
{
    MapWorld world(8, 8, true);
    CommandQueue commands;

    std::vector<int> order;
    commands.On<SpawnCommand>([&order](MapWorld&, const SpawnCommand& command) { order.push_back(command.x); });

    for (int i = 0; i < 5; ++i)
    {
        commands.Push(SpawnCommand{i, 0});
    }

    commands.Drain(world);

    const std::vector<int> expected{0, 1, 2, 3, 4};
    CHECK(order == expected);
}

void StaleHandlesAreRejectedNotMisapplied()
{
    MapWorld world(8, 8, true);
    CommandQueue commands;

    commands.On<MoveCommand>(
        [](MapWorld& target, const MoveCommand& command)
        {
            if (!target.registry.Exists(command.entity))
            {
                return;
            }
            target.registry.Assign<MoveIntentComponent>(command.entity, command.directionX, command.directionY);
        });

    const Entity first = world.Spawn(2, 2);

    // A packet arrives, then the player disconnects before the tick runs.
    commands.Push(MoveCommand{first, 1, 0});
    world.Despawn(first);

    // A different entity takes over the freed slot.
    const Entity second = world.Spawn(2, 2);
    CHECK(second != kNullEntity);
    CHECK_EQ(EntityIndex(second), EntityIndex(first));

    commands.Drain(world);

    // The generation in the handle is what stops the queued command from
    // steering whoever inherited the slot.
    CHECK(!world.registry.Has<MoveIntentComponent>(second));
}

void UnhandledCommandsAreCountedNotSilent()
{
    MapWorld world(4, 4, true);
    CommandQueue commands;

    commands.Push(UnwiredCommand{1});
    commands.Push(UnwiredCommand{2});

    CHECK_EQ(commands.Drain(world), 2u);

    // A packet that reached the queue and did nothing is a wiring bug, so
    // it has to be observable rather than swallowed.
    CHECK_EQ(commands.UnhandledCount(), 2u);
}

void HandlerRegisteredLastWins()
{
    MapWorld world(4, 4, true);
    CommandQueue commands;

    int a = 0;
    int b = 0;
    commands.On<SpawnCommand>([&a](MapWorld&, const SpawnCommand&) { ++a; });
    commands.On<SpawnCommand>([&b](MapWorld&, const SpawnCommand&) { ++b; });

    commands.Push(SpawnCommand{0, 0});
    commands.Drain(world);

    // Two independent readings of the same inbound command is a mistake,
    // not a feature -- the second registration replaces the first.
    CHECK_EQ(a, 0);
    CHECK_EQ(b, 1);
    CHECK_EQ(commands.UnhandledCount(), 0u);
}

void CommandsPushedDuringDrainWaitForNextTick()
{
    MapWorld world(8, 8, true);
    CommandQueue commands;

    int spawns = 0;
    commands.On<SpawnCommand>(
        [&](MapWorld&, const SpawnCommand& command)
        {
            ++spawns;
            if (command.x == 0)
            {
                // A handler re-queuing work must not extend the batch it is
                // already inside -- a tick's inbound set is fixed once the
                // drain starts.
                commands.Push(SpawnCommand{1, 0});
            }
        });

    commands.Push(SpawnCommand{0, 0});

    CHECK_EQ(commands.Drain(world), 1u);
    CHECK_EQ(spawns, 1);
    CHECK_EQ(commands.PendingCount(), 1u);

    CHECK_EQ(commands.Drain(world), 1u);
    CHECK_EQ(spawns, 2);
    CHECK_EQ(commands.PendingCount(), 0u);
}

void ClearDiscardsWithoutApplying()
{
    MapWorld world(4, 4, true);
    CommandQueue commands;

    int applied = 0;
    commands.On<SpawnCommand>([&applied](MapWorld&, const SpawnCommand&) { ++applied; });

    commands.Push(SpawnCommand{0, 0});
    commands.Clear();

    CHECK_EQ(commands.PendingCount(), 0u);
    CHECK_EQ(commands.Drain(world), 0u);
    CHECK_EQ(applied, 0);
}

void ConcurrentPushersLoseNothing()
{
    MapWorld world(4, 4, true);
    CommandQueue commands;

    std::atomic<int> applied{0};
    commands.On<SpawnCommand>([&applied](MapWorld&, const SpawnCommand&) { applied.fetch_add(1); });

    // The reason this class exists: many connection threads pushing at once
    // while the simulation thread is otherwise occupied. Every command must
    // survive, exactly once.
    constexpr int kThreads = 8;
    constexpr int kPerThread = 500;

    std::vector<std::thread> pushers;
    pushers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        pushers.emplace_back(
            [&commands, t]()
            {
                for (int i = 0; i < kPerThread; ++i)
                {
                    commands.Push(SpawnCommand{t, i});
                }
            });
    }

    for (std::thread& pusher : pushers)
    {
        pusher.join();
    }

    CHECK_EQ(commands.PendingCount(), static_cast<std::size_t>(kThreads * kPerThread));
    CHECK_EQ(commands.Drain(world), static_cast<std::size_t>(kThreads * kPerThread));
    CHECK_EQ(applied.load(), kThreads * kPerThread);
}

void DrainInterleavedWithLivePushers()
{
    MapWorld world(4, 4, true);
    CommandQueue commands;

    std::atomic<int> applied{0};
    commands.On<SpawnCommand>([&applied](MapWorld&, const SpawnCommand&) { applied.fetch_add(1); });

    constexpr int kThreads = 4;
    constexpr int kPerThread = 1000;

    std::atomic<bool> producing{true};
    std::vector<std::thread> pushers;
    pushers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        pushers.emplace_back(
            [&commands, t]()
            {
                for (int i = 0; i < kPerThread; ++i)
                {
                    commands.Push(SpawnCommand{t, i});
                }
            });
    }

    // Drain repeatedly while they are still pushing -- the realistic shape,
    // and the one where a swap-under-lock bug would drop or double-apply.
    while (producing.load())
    {
        commands.Drain(world);
        if (applied.load() >= kThreads * kPerThread)
        {
            producing.store(false);
        }
    }

    for (std::thread& pusher : pushers)
    {
        pusher.join();
    }

    // Anything pushed after the last drain is still queued; the invariant is
    // that applied plus pending accounts for every command, with none lost
    // and none run twice.
    commands.Drain(world);
    CHECK_EQ(applied.load(), kThreads * kPerThread);
    CHECK_EQ(commands.PendingCount(), 0u);
}

} // namespace

int main()
{
    PushDefersUntilDrain();
    CommandsApplyInPushOrder();
    StaleHandlesAreRejectedNotMisapplied();
    UnhandledCommandsAreCountedNotSilent();
    HandlerRegisteredLastWins();
    CommandsPushedDuringDrainWaitForNextTick();
    ClearDiscardsWithoutApplying();
    ConcurrentPushersLoseNothing();
    DrainInterleavedWithLivePushers();

    return world_v2::test::Summary("CommandQueue");
}
