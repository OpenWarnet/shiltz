#include "../Simulation.h"
#include "../system/BroadcastModule.h"
#include "../system/CoreSimulationModule.h"
#include "Module.h"
#include "System.h"
#include "Test.h"

#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <crtdbg.h>
#include <stdlib.h>
#endif

using namespace world_v2;

namespace
{

// Where the systems and modules below record what ran, in the order it ran.
// Every ordering claim in this file is checked against this one vector.
std::vector<std::string> g_log;

// --------------------------------------------------------------------
// Test doubles
// --------------------------------------------------------------------

template <int Tag>
class MarkerSystem : public ISystem
{
public:
    const char* Name() const override
    {
        return "MarkerSystem";
    }

    void Run(Map&, float) override
    {
        g_log.push_back(label);
        ++runs;
    }

    std::string label = "marker";
    int runs = 0;
};

using SystemA = MarkerSystem<1>;
using SystemB = MarkerSystem<2>;
using SystemC = MarkerSystem<3>;

// Records Setup / Start / Shutdown so their relative order is checkable.
class TracerModule : public Module
{
public:
    explicit TracerModule(std::string name)
        : m_name(std::move(name))
    {
    }

    const char* Name() const override
    {
        return m_name.c_str();
    }

    void Setup(ModuleContext&) override
    {
        g_log.push_back(m_name + ":setup");
    }

    void Start(Simulation&) override
    {
        g_log.push_back(m_name + ":start");
    }

    void Shutdown() override
    {
        g_log.push_back(m_name + ":shutdown");
    }

private:
    std::string m_name;
};

struct PokeCommand
{
    ConnectionId connection = kInvalidConnection;
    int value = 0;
};

struct BareCommand
{
    int value = 0;
};

// --------------------------------------------------------------------
// Setup / Start ordering
// --------------------------------------------------------------------

// Every Setup runs before any Start. This is the property that retires the
// old "install the rules, place the spawners, then prime, in that order"
// contract: a module's Start can depend on any other module's listeners
// being live, whatever order they were installed in.
void SetupRunsForEveryModuleBeforeAnyStart()
{
    g_log.clear();

    {
        Simulation simulation(8, 8, true);
        simulation.Install<TracerModule>("first");
        simulation.Install<TracerModule>("second");

        // Setup is immediate; Start has not happened yet.
        CHECK_EQ(g_log.size(), std::size_t{2});
        CHECK(g_log[0] == "first:setup");
        CHECK(g_log[1] == "second:setup");

        simulation.Tick(0.1f);

        CHECK_EQ(g_log.size(), std::size_t{4});
        CHECK(g_log[2] == "first:start");
        CHECK(g_log[3] == "second:start");
    }

    // Shutdown, in reverse install order, at destruction.
    CHECK_EQ(g_log.size(), std::size_t{6});
    CHECK(g_log[4] == "second:shutdown");
    CHECK(g_log[5] == "first:shutdown");
}

void StartIsIdempotentAndTickDoesNotRepeatIt()
{
    g_log.clear();

    Simulation simulation(8, 8, true);
    simulation.Install<TracerModule>("only");

    simulation.Start();
    simulation.Start();
    simulation.Tick(0.1f);
    simulation.Tick(0.1f);

    int starts = 0;
    for (const std::string& entry : g_log)
    {
        if (entry == "only:start")
        {
            ++starts;
        }
    }

    CHECK_EQ(starts, 1);
}

// --------------------------------------------------------------------
// Declaration order
// --------------------------------------------------------------------

// The whole contract: stage 2 runs in the order the modules declared it,
// across module boundaries as well as within one Setup.
void SystemsRunInDeclarationOrderAcrossModules()
{
    g_log.clear();

    Simulation simulation(8, 8, true);

    simulation.Install<LambdaModule>("alpha", [](ModuleContext& ctx) {
        ctx.AddSystem<SystemA>().Get().label = "A";
        ctx.AddSystem<SystemB>().Get().label = "B";
    });

    simulation.Install<LambdaModule>("beta", [](ModuleContext& ctx) {
        ctx.AddSystem<SystemC>().Get().label = "C";
    });

    simulation.Tick(0.1f);

    CHECK_EQ(g_log.size(), std::size_t{3});
    CHECK(g_log[0] == "A");
    CHECK(g_log[1] == "B");
    CHECK(g_log[2] == "C");
}

// Installing in the other order really does change the tick order -- the
// cost of declaration order, stated as a test so it is not a surprise.
void InstallOrderDecidesTickOrder()
{
    g_log.clear();

    Simulation simulation(8, 8, true);

    simulation.Install<LambdaModule>("beta", [](ModuleContext& ctx) {
        ctx.AddSystem<SystemC>().Get().label = "C";
    });

    simulation.Install<LambdaModule>("alpha", [](ModuleContext& ctx) {
        ctx.AddSystem<SystemA>().Get().label = "A";
    });

    simulation.Tick(0.1f);

    CHECK_EQ(g_log.size(), std::size_t{2});
    CHECK(g_log[0] == "C");
    CHECK(g_log[1] == "A");
}

// The handle is what makes the previously unreachable knobs reachable.
void AddSystemReturnsATunableHandle()
{
    Simulation simulation(8, 8, true);

    SystemA* captured = nullptr;
    simulation.Install<LambdaModule>("tuner", [&captured](ModuleContext& ctx) {
        SystemHandle<SystemA> handle = ctx.AddSystem<SystemA>();
        handle.Get().label = "tuned";
        captured = &handle.Get();
    });

    CHECK(captured != nullptr);
    CHECK(captured->label == "tuned");
    CHECK_EQ(simulation.FindSystem<SystemA>(), captured);
}

// A satisfied After<T> is silent -- the check only ever speaks up when the
// declared order is actually wrong.
void SatisfiedAfterConstraintPasses()
{
    g_log.clear();

    Simulation simulation(8, 8, true);
    simulation.Install<LambdaModule>("ordered", [](ModuleContext& ctx) {
        ctx.AddSystem<SystemA>().Get().label = "A";
        ctx.AddSystem<SystemB>().After<SystemA>().Get().label = "B";
    });

    simulation.Tick(0.1f);

    CHECK_EQ(g_log.size(), std::size_t{2});
    CHECK(g_log[0] == "A");
    CHECK(g_log[1] == "B");
}

// --------------------------------------------------------------------
// Outbound
// --------------------------------------------------------------------

// Multicast and ordered, unlike the single OnBroadcast slot it replaces.
void OutboundHandlersAreMulticastAndOrdered()
{
    g_log.clear();

    Simulation simulation(8, 8, true);

    simulation.Install<LambdaModule>("first", [](ModuleContext& ctx) {
        ctx.AddOutbound([](Map&) { g_log.push_back("out1"); });
    });

    simulation.Install<LambdaModule>("second", [](ModuleContext& ctx) {
        ctx.AddOutbound([](Map&) { g_log.push_back("out2"); });
    });

    simulation.Tick(0.1f);

    CHECK_EQ(g_log.size(), std::size_t{2});
    CHECK(g_log[0] == "out1");
    CHECK(g_log[1] == "out2");
}

// Stage 4 runs after the barrier: anything the flush settled is visible.
void OutboundSeesTheSettledWorld()
{
    Simulation simulation(8, 8, true);

    bool sawEntity = false;
    simulation.Install<LambdaModule>("watcher", [&sawEntity](ModuleContext& ctx) {
        Map& world = ctx.World();
        ctx.Listen<int>([&world](const int&) { world.Spawn(2, 2); });
        ctx.AddOutbound([&sawEntity](Map& map) { sawEntity = map.registry.AliveCount() > 0; });
    });

    simulation.World().events.Emit(7);
    simulation.Tick(0.1f);

    CHECK(sawEntity);
}

// --------------------------------------------------------------------
// Commands
// --------------------------------------------------------------------

void OnPlayerCommandResolvesTheSender()
{
    Simulation simulation(8, 8, true);

    Entity seen = kNullEntity;
    int seenValue = 0;

    simulation.Install<LambdaModule>("player", [&seen, &seenValue](ModuleContext& ctx) {
        ctx.OnSpawnPlayer([](Map& map, const JoinCommand& command) { return map.Spawn(command.x, command.y); });

        ctx.OnPlayerCommand<PokeCommand>([&seen, &seenValue](Map&, Entity actor, const PokeCommand& command) {
            seen = actor;
            seenValue = command.value;
        });
    });

    simulation.Commands().Push(JoinCommand{1, 100, 3, 3});
    simulation.Tick(0.1f);

    const Entity player = simulation.EntityFor(1);
    CHECK(player != kNullEntity);

    simulation.Commands().Push(PokeCommand{1, 42});
    simulation.Tick(0.1f);

    CHECK_EQ(seen, player);
    CHECK_EQ(seenValue, 42);
}

// A command from a connection that is no longer here is counted and
// dropped, not delivered at whoever inherited the slot.
void OrphanedPlayerCommandsAreCounted()
{
    Simulation simulation(8, 8, true);

    int delivered = 0;
    simulation.Install<LambdaModule>("player", [&delivered](ModuleContext& ctx) {
        ctx.OnPlayerCommand<PokeCommand>([&delivered](Map&, Entity, const PokeCommand&) { ++delivered; });
    });

    simulation.Commands().Push(PokeCommand{99, 1});
    simulation.Tick(0.1f);

    CHECK_EQ(delivered, 0);
    CHECK_EQ(simulation.OrphanedCommandCount(), std::size_t{1});
}

// A command with no connection at all still works through OnCommand.
void RawCommandsBypassSenderResolution()
{
    Simulation simulation(8, 8, true);

    int value = 0;
    simulation.Install<LambdaModule>("raw", [&value](ModuleContext& ctx) {
        ctx.OnCommand<BareCommand>([&value](Map&, const BareCommand& command) { value = command.value; });
    });

    simulation.Commands().Push(BareCommand{5});
    simulation.Tick(0.1f);

    CHECK_EQ(value, 5);
}

// --------------------------------------------------------------------
// Lookup
// --------------------------------------------------------------------

void ModulesFindEachOther()
{
    Simulation simulation(8, 8, true);

    CoreSimulationModule& core = simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();

    CHECK_EQ(simulation.Find<CoreSimulationModule>(), &core);
    CHECK(simulation.Find<BroadcastModule>() != nullptr);
    CHECK_EQ(simulation.Find<TracerModule>(), nullptr);

    // Systems too, which is how SpawnRulesModule reaches the SpawnSystem
    // that CoreSimulationModule owns.
    CHECK_EQ(simulation.FindSystem<AISystem>(), &core.AI());
    CHECK_EQ(simulation.FindSystem<SystemA>(), nullptr);
}

// The knobs that nothing routed through a Simulation could reach before.
void CoreModuleExposesSystemTunables()
{
    Simulation simulation(8, 8, true);

    CoreSimulationModule& core = simulation.Install<CoreSimulationModule>();
    core.AI().defaultAttackDamage = 3;
    core.Movement().tilesPerSecond = 6.0f;
    core.Pickup().pickupRange = 2;

    CHECK_EQ(simulation.FindSystem<AISystem>()->defaultAttackDamage, 3);
    CHECK_EQ(simulation.FindSystem<PickupSystem>()->pickupRange, 2);
}

// --------------------------------------------------------------------
// Fatal scenarios, run in a child process
//
// Each of these trips an assertion, which aborts. There is no in-process
// way to check for that, so the test re-runs itself with the scenario name
// as an argument and asserts the child died. Positive control included, so
// a harness that silently never runs anything cannot pass.
// --------------------------------------------------------------------

void FatalDuplicateCommand()
{
    Simulation simulation(8, 8, true);
    simulation.Install<LambdaModule>("one", [](ModuleContext& ctx) {
        ctx.OnCommand<BareCommand>([](Map&, const BareCommand&) {});
    });
    simulation.Install<LambdaModule>("two", [](ModuleContext& ctx) {
        ctx.OnCommand<BareCommand>([](Map&, const BareCommand&) {});
    });
}

void FatalReservedCommand()
{
    Simulation simulation(8, 8, true);
    simulation.Install<LambdaModule>("greedy", [](ModuleContext& ctx) {
        ctx.OnCommand<JoinCommand>([](Map&, const JoinCommand&) {});
    });
}

void FatalInstallAfterStart()
{
    Simulation simulation(8, 8, true);
    simulation.Tick(0.1f);
    simulation.Install<TracerModule>("late");
}

void FatalViolatedAfterConstraint()
{
    Simulation simulation(8, 8, true);
    simulation.Install<LambdaModule>("backwards", [](ModuleContext& ctx) {
        // B declares it needs A first, but A is added second.
        ctx.AddSystem<SystemB>().After<SystemA>();
        ctx.AddSystem<SystemA>();
    });
    simulation.Tick(0.1f);
}

void FatalTwoSpawners()
{
    Simulation simulation(8, 8, true);
    simulation.Install<LambdaModule>("one", [](ModuleContext& ctx) {
        ctx.OnSpawnPlayer([](Map&, const JoinCommand&) { return kNullEntity; });
    });
    simulation.Install<LambdaModule>("two", [](ModuleContext& ctx) {
        ctx.OnSpawnPlayer([](Map&, const JoinCommand&) { return kNullEntity; });
    });
}

// The positive control: does nothing fatal, so the child must exit 0.
void FatalControlSurvives()
{
    Simulation simulation(8, 8, true);
    simulation.Install<TracerModule>("fine");
    simulation.Tick(0.1f);
}

struct Scenario
{
    const char* name;
    void (*run)();
    bool shouldDie;
};

const Scenario kScenarios[] = {
    {"duplicate-command", FatalDuplicateCommand, true},
    {"reserved-command", FatalReservedCommand, true},
    {"install-after-start", FatalInstallAfterStart, true},
    {"violated-after", FatalViolatedAfterConstraint, true},
    {"two-spawners", FatalTwoSpawners, true},
    {"control", FatalControlSurvives, false},
};

std::string g_executable;

void FatalScenariosAbort()
{
#ifdef NDEBUG
    // The assertions these check are debug-only by design, so there is
    // nothing to observe in a release build.
    std::printf("  SKIP  fatal scenarios (release build)\n");
#else
    if (g_executable.empty())
    {
        std::printf("  SKIP  fatal scenarios (no argv[0])\n");
        return;
    }

    for (const Scenario& scenario : kScenarios)
    {
        // cmd.exe strips the outermost pair of quotes, so the executable
        // path needs its own set to survive a directory containing spaces.
        const std::string command = "\"\"" + g_executable + "\" " + scenario.name + "\" >NUL 2>NUL";
        const int status = std::system(command.c_str());

        if (scenario.shouldDie)
        {
            CHECK(status != 0);
        }
        else
        {
            CHECK_EQ(status, 0);
        }

        if ((status != 0) != scenario.shouldDie)
        {
            std::printf("        scenario '%s' returned %d\n", scenario.name, status);
        }
    }
#endif
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 0 && argv[0] != nullptr)
    {
        g_executable = argv[0];
    }

    // Child mode: run one scenario and let the assertion do what it does.
    if (argc > 1)
    {
#ifdef _WIN32
        // Without this a failed assert opens a modal dialog and the parent
        // waits forever. Send it to stderr and abort instead.
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

        for (const Scenario& scenario : kScenarios)
        {
            if (std::string(argv[1]) == scenario.name)
            {
                scenario.run();
                return 0;
            }
        }

        return 2;
    }

    SetupRunsForEveryModuleBeforeAnyStart();
    StartIsIdempotentAndTickDoesNotRepeatIt();

    SystemsRunInDeclarationOrderAcrossModules();
    InstallOrderDecidesTickOrder();
    AddSystemReturnsATunableHandle();
    SatisfiedAfterConstraintPasses();

    OutboundHandlersAreMulticastAndOrdered();
    OutboundSeesTheSettledWorld();

    OnPlayerCommandResolvesTheSender();
    OrphanedPlayerCommandsAreCounted();
    RawCommandsBypassSenderResolution();

    ModulesFindEachOther();
    CoreModuleExposesSystemTunables();

    FatalScenariosAbort();

    return world_v2::test::Summary("Module");
}
