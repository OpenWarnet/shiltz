#pragma once

#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Items.h"
#include "../core/Entity.h"
#include "../core/Map.h"
#include "../core/Module.h"
#include "../event/CombatEvents.h"

#include <cstdint>

namespace world_v2
{

// How many levels one experience award may grant before it stops.
//
// A guard, not a rule. The loop below subtracts the threshold and levels
// again while the remainder still clears it, which terminates on any sane
// curve -- but a game that leaves requiredForNextLevel unchanged after a
// level-up would otherwise spin here. The cap turns that mistake into a
// stuck level rather than a hung server.
inline constexpr int kMaxLevelsPerAward = 8;

// The default resolution of what the combat systems announce.
//
// This is game policy sitting on top of the framework, not part of it. The
// systems decide *that* something died; this decides what dying costs and
// pays. It is a module so a game with different rules can install its own
// instead, and so the ordering below is readable in one place.
//
// Everything here runs inside EventManager::Flush, at the barrier, which is
// the only point in the tick where despawning and spawning are legal.
//
// The cascade
// -----------
//   DeathEvent            -> credits the killer, announces the drop,
//                            despawns the corpse
//   ExperienceAwardEvent  -> applies experience, levels up if it clears
//                            the threshold
//   LevelUpEvent          -> nothing here; the new threshold comes from
//                            level.scr, which the game layer owns
//   LootDropEvent         -> nothing here; turning a table id into items
//                            needs .scr data the framework does not have
//
// Each link is emitted rather than called, so all of it settles in one
// Flush -- a kill resolves completely within the tick that caused it,
// instead of one step per tick.
//
// The two open links are the seam a game layer fills with its own module,
// installed after this one.
class CombatRulesModule : public Module
{
public:
    const char* Name() const override
    {
        return "CombatRules";
    }

    void Setup(ModuleContext& context) override
    {
        Map& world = context.World();

        context.Listen<DeathEvent>(
            [&world](const DeathEvent& event)
            {
                // An earlier handler in this same flush may already have
                // removed it.
                if (!world.registry.Exists(event.entity))
                {
                    return;
                }

                // Read what is still needed off the corpse before
                // despawning it. Where it fell comes from the event rather
                // than the registry, so this handler's position in the
                // listener order does not matter.
                const ExperienceRewardComponent* reward =
                    world.registry.TryGet<ExperienceRewardComponent>(event.entity);
                if (reward != nullptr && reward->amount > 0 && world.registry.Exists(event.killer) &&
                    !world.registry.Has<DeadComponent>(event.killer) &&
                    world.registry.Has<ExperienceComponent>(event.killer))
                {
                    // The killer may have died in the same tick. Exists
                    // alone is not enough to rule that out -- a corpse is
                    // still present until its own DeathEvent is handled,
                    // and which death is dispatched first is just emit
                    // order. Checking the tag means a mutual kill pays
                    // neither side, rather than paying whichever one
                    // happened to die second.
                    world.events.Emit(ExperienceAwardEvent{event.killer, reward->amount});
                }

                const LootTableComponent* loot = world.registry.TryGet<LootTableComponent>(event.entity);
                if (loot != nullptr && loot->dropTableId != 0)
                {
                    world.events.Emit(LootDropEvent{event.entity, loot->dropTableId, event.x, event.y});
                }

                // Releases the tile and destroys the entity. Legal here and
                // nowhere else in the tick.
                world.Despawn(event.entity);
            });

        context.Listen<ExperienceAwardEvent>(
            [&world](const ExperienceAwardEvent& event)
            {
                if (!world.registry.Exists(event.entity))
                {
                    return;
                }

                ExperienceComponent* experience = world.registry.TryGet<ExperienceComponent>(event.entity);
                if (experience == nullptr)
                {
                    return;
                }

                experience->current += event.amount;

                int granted = 0;
                while (experience->requiredForNextLevel > 0 &&
                       experience->current >= experience->requiredForNextLevel && granted < kMaxLevelsPerAward)
                {
                    experience->current -= experience->requiredForNextLevel;
                    ++experience->level;
                    ++granted;

                    // Deferred like everything else, so the game layer's
                    // listener -- the one that sets the next threshold from
                    // level.scr -- runs in the following flush round rather
                    // than inside this loop.
                    world.events.Emit(LevelUpEvent{event.entity, experience->level});
                }
            });
    }
};

} // namespace world_v2
