#include "EventManager.h"
#include "Test.h"

#include <string>
#include <vector>

using namespace world_v2;

namespace
{

struct DeathEvent
{
    int entityId = 0;
};

struct LootDropEvent
{
    int itemId = 0;
};

struct LevelUpEvent
{
    int level = 0;
};

void EmitDefersUntilFlush()
{
    EventManager events;

    int handled = 0;
    events.Listen<DeathEvent>([&handled](const DeathEvent&) { ++handled; });

    events.Emit(DeathEvent{1});
    events.Emit(DeathEvent{2});

    // Nothing may run before the barrier -- that is the entire contract.
    CHECK_EQ(handled, 0);
    CHECK_EQ(events.PendingCount(), 2u);

    events.Flush();

    CHECK_EQ(handled, 2);
    CHECK_EQ(events.PendingCount(), 0u);

    // A second Flush with nothing queued does nothing.
    events.Flush();
    CHECK_EQ(handled, 2);
}

void EventDataSurvivesTheQueue()
{
    EventManager events;

    std::vector<int> ids;
    events.Listen<DeathEvent>([&ids](const DeathEvent& event) { ids.push_back(event.entityId); });

    {
        // Emitted from a scope that is gone by the time Flush runs -- the
        // queue holds a copy, not a reference.
        DeathEvent local{42};
        events.Emit(local);
    }

    events.Flush();

    CHECK_EQ(ids.size(), 1u);
    if (ids.size() == 1)
    {
        CHECK_EQ(ids[0], 42);
    }
}

void OrderIsPreservedAcrossTypes()
{
    EventManager events;

    std::vector<std::string> log;
    events.Listen<DeathEvent>([&log](const DeathEvent& e) { log.push_back("death:" + std::to_string(e.entityId)); });
    events.Listen<LootDropEvent>([&log](const LootDropEvent& e) { log.push_back("loot:" + std::to_string(e.itemId)); });
    events.Listen<LevelUpEvent>([&log](const LevelUpEvent& e) { log.push_back("level:" + std::to_string(e.level)); });

    events.Emit(DeathEvent{1});
    events.Emit(LootDropEvent{100});
    events.Emit(LevelUpEvent{7});
    events.Emit(DeathEvent{2});
    events.Emit(LootDropEvent{200});

    events.Flush();

    // Interleaved types come back out in emit order, not grouped by type
    // and not in whatever order a hash map would have produced.
    const std::vector<std::string> expected{"death:1", "loot:100", "level:7", "death:2", "loot:200"};
    CHECK(log == expected);
}

void ListenersFireInRegistrationOrder()
{
    EventManager events;

    std::vector<int> order;
    events.Listen<DeathEvent>([&order](const DeathEvent&) { order.push_back(1); });
    events.Listen<DeathEvent>([&order](const DeathEvent&) { order.push_back(2); });
    events.Listen<DeathEvent>([&order](const DeathEvent&) { order.push_back(3); });

    events.Emit(DeathEvent{0});
    events.Flush();

    const std::vector<int> expected{1, 2, 3};
    CHECK(order == expected);
}

void UnlistenedEventsAreHarmless()
{
    EventManager events;

    // Nothing is listening for LootDropEvent. Emitting it is normal --
    // a system announces what happened regardless of who cares.
    events.Emit(LootDropEvent{5});
    CHECK_EQ(events.PendingCount(), 1u);

    events.Flush();
    CHECK_EQ(events.PendingCount(), 0u);
}

void CascadesResolveInOneFlush()
{
    EventManager events;

    std::vector<std::string> log;

    // death -> loot drop -> level up, each link emitted by the previous
    // link's handler. All three must settle inside a single barrier rather
    // than trickling out one per tick.
    events.Listen<DeathEvent>(
        [&](const DeathEvent& event)
        {
            log.push_back("death");
            events.Emit(LootDropEvent{event.entityId * 10});
        });

    events.Listen<LootDropEvent>(
        [&](const LootDropEvent&)
        {
            log.push_back("loot");
            events.Emit(LevelUpEvent{2});
        });

    events.Listen<LevelUpEvent>([&](const LevelUpEvent&) { log.push_back("level"); });

    events.Emit(DeathEvent{3});
    events.Flush();

    const std::vector<std::string> expected{"death", "loot", "level"};
    CHECK(log == expected);
    CHECK_EQ(events.PendingCount(), 0u);
}

void RunawayCascadeIsBounded()
{
    EventManager events;

    // A handler that re-emits its own event forever. Flush must return
    // after kMaxFlushRounds instead of spinning.
    int handled = 0;
    events.Listen<DeathEvent>(
        [&](const DeathEvent& event)
        {
            ++handled;
            events.Emit(DeathEvent{event.entityId + 1});
        });

    events.Emit(DeathEvent{0});
    events.Flush();

    CHECK_EQ(handled, EventManager::kMaxFlushRounds);

    // The event still in flight is left queued rather than dropped, so a
    // deep-but-terminating cascade degrades to a delay instead of a loss.
    CHECK_EQ(events.PendingCount(), 1u);
}

void ClearDiscardsWithoutDispatching()
{
    EventManager events;

    int handled = 0;
    events.Listen<DeathEvent>([&handled](const DeathEvent&) { ++handled; });

    events.Emit(DeathEvent{1});
    events.Clear();
    CHECK_EQ(events.PendingCount(), 0u);

    events.Flush();
    CHECK_EQ(handled, 0);
}

} // namespace

int main()
{
    EmitDefersUntilFlush();
    EventDataSurvivesTheQueue();
    OrderIsPreservedAcrossTypes();
    ListenersFireInRegistrationOrder();
    UnlistenedEventsAreHarmless();
    CascadesResolveInOneFlush();
    RunawayCascadeIsBounded();
    ClearDiscardsWithoutDispatching();

    return world_v2::test::Summary("EventManager");
}
