#include "Simulation.h"
#include "component/Grid.h"
#include "core/Entity.h"
#include "core/Test.h"

#include <string>
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

struct NoteEvent
{
    int value = 0;
};

void WireMovement(Simulation& simulation)
{
    simulation.Commands().On<MoveCommand>(
        [](MapWorld& world, const MoveCommand& command)
        {
            if (!world.registry.Exists(command.entity))
            {
                return;
            }
            world.registry.Assign<MoveIntentComponent>(command.entity, command.directionX, command.directionY);
        });
}

void PacketToMovementInOneTick()
{
    Simulation simulation(8, 8, true);
    WireMovement(simulation);

    const Entity entity = simulation.World().SpawnBlocking(2, 2);

    // What a connection thread does: push and walk away.
    simulation.Commands().Push(MoveCommand{entity, 1, 0});

    simulation.Tick(0.0f);

    CHECK_EQ(simulation.LastCommandCount(), 1u);

    // Stage 1 turned the command into an intent and stage 2 consumed it,
    // all inside one tick.
    const GridPositionComponent& position = simulation.World().registry.Get<GridPositionComponent>(entity);
    CHECK_EQ(position.x, 3);
    CHECK_EQ(position.y, 2);
    CHECK_EQ(simulation.World().tiles.OccupantAt(3, 2), entity);
    CHECK(!simulation.World().registry.Has<MoveIntentComponent>(entity));
}

void StagesRunInOrder()
{
    Simulation simulation(8, 8, true);

    std::vector<std::string> log;
    int positionWhenEventFired = -1;

    const Entity entity = simulation.World().SpawnBlocking(2, 2);

    // Stage 1: the command handler both creates the intent and queues an
    // event for the barrier.
    simulation.Commands().On<MoveCommand>(
        [&log](MapWorld& world, const MoveCommand& command)
        {
            log.push_back("command");
            world.registry.Assign<MoveIntentComponent>(command.entity, command.directionX, command.directionY);
            world.events.Emit(NoteEvent{1});
        });

    // Stage 3: fires at the barrier, which is after the simulation stage.
    simulation.World().events.Listen<NoteEvent>(
        [&](const NoteEvent&)
        {
            log.push_back("event");
            positionWhenEventFired = simulation.World().registry.Get<GridPositionComponent>(entity).x;
        });

    // Stage 4.
    simulation.OnBroadcast([&log](MapWorld&) { log.push_back("broadcast"); });

    simulation.Commands().Push(MoveCommand{entity, 1, 0});
    simulation.Tick(0.0f);

    const std::vector<std::string> expected{"command", "event", "broadcast"};
    CHECK(log == expected);

    // The barrier must observe a settled simulation: movement had already
    // committed by the time the event ran.
    CHECK_EQ(positionWhenEventFired, 3);
}

void EventsEmittedInAStageResolveThisTickNotNext()
{
    Simulation simulation(4, 4, true);

    int flushed = 0;
    simulation.World().events.Listen<NoteEvent>([&flushed](const NoteEvent& event) { flushed += event.value; });

    simulation.Commands().On<MoveCommand>([](MapWorld& world, const MoveCommand&) { world.events.Emit(NoteEvent{7}); });

    simulation.Commands().Push(MoveCommand{});
    simulation.Tick(0.0f);

    // One tick, not two: the barrier is inside the same tick that queued it.
    CHECK_EQ(flushed, 7);
    CHECK_EQ(simulation.World().events.PendingCount(), 0u);
}

void CommandsPushedDuringBroadcastLandNextTick()
{
    Simulation simulation(8, 8, true);
    WireMovement(simulation);

    const Entity entity = simulation.World().SpawnBlocking(2, 2);

    bool pushed = false;
    simulation.OnBroadcast(
        [&](MapWorld&)
        {
            if (!pushed)
            {
                pushed = true;
                simulation.Commands().Push(MoveCommand{entity, 1, 0});
            }
        });

    simulation.Tick(0.0f);

    // Stage 4 runs after stage 1 has already closed, so this cannot
    // retroactively join the tick that queued it.
    CHECK_EQ(simulation.LastCommandCount(), 0u);
    CHECK_EQ(simulation.World().registry.Get<GridPositionComponent>(entity).x, 2);

    simulation.Tick(0.0f);
    CHECK_EQ(simulation.LastCommandCount(), 1u);
    CHECK_EQ(simulation.World().registry.Get<GridPositionComponent>(entity).x, 3);
}

void EmptyTicksAreHarmless()
{
    Simulation simulation(4, 4, true);

    for (int i = 0; i < 10; ++i)
    {
        simulation.Tick(0.016f);
        CHECK_EQ(simulation.LastCommandCount(), 0u);
    }

    CHECK_EQ(simulation.World().registry.AliveCount(), 0u);
}

void ContinuousMovementAcrossTicks()
{
    Simulation simulation(16, 16, true);
    WireMovement(simulation);

    const Entity entity = simulation.World().SpawnBlocking(0, 5);

    // A player holding a direction: one command per tick, at the default 4
    // tiles/second with a 0.25s step, so each tick completes a step and
    // frees the entity for the next.
    for (int i = 0; i < 6; ++i)
    {
        simulation.Commands().Push(MoveCommand{entity, 1, 0});
        simulation.Tick(0.25f);
    }

    const GridPositionComponent& position = simulation.World().registry.Get<GridPositionComponent>(entity);
    CHECK_EQ(position.x, 6);
    CHECK_EQ(position.y, 5);

    // The trail behind it must be clear -- every intermediate tile was
    // released as it was left.
    for (int x = 0; x < 6; ++x)
    {
        CHECK_EQ(simulation.World().tiles.OccupantAt(x, 5), kNullEntity);
    }
    CHECK_EQ(simulation.World().tiles.OccupantAt(6, 5), entity);
}

void SeparateMapsDoNotShareState()
{
    Simulation first(8, 8, true);
    Simulation second(8, 8, true);
    WireMovement(first);
    WireMovement(second);

    const Entity a = first.World().SpawnBlocking(2, 2);
    const Entity b = second.World().SpawnBlocking(2, 2);

    first.Commands().Push(MoveCommand{a, 1, 0});
    first.Tick(0.0f);
    second.Tick(0.0f);

    // Same coordinates, same handle values, entirely separate worlds.
    CHECK_EQ(first.World().registry.Get<GridPositionComponent>(a).x, 3);
    CHECK_EQ(second.World().registry.Get<GridPositionComponent>(b).x, 2);
    CHECK_EQ(second.LastCommandCount(), 0u);
    CHECK_EQ(first.World().tiles.OccupantAt(2, 2), kNullEntity);
    CHECK_EQ(second.World().tiles.OccupantAt(2, 2), b);
}

} // namespace

int main()
{
    PacketToMovementInOneTick();
    StagesRunInOrder();
    EventsEmittedInAStageResolveThisTickNotNext();
    CommandsPushedDuringBroadcastLandNextTick();
    EmptyTicksAreHarmless();
    ContinuousMovementAcrossTicks();
    SeparateMapsDoNotShareState();

    return world_v2::test::Summary("Simulation");
}
