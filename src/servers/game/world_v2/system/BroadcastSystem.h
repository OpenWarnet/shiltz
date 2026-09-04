#pragma once

#include "../component/Grid.h"
#include "../component/Network.h"
#include "../core/Entity.h"
#include "../core/Registry.h"
#include "../event/CombatEvents.h"
#include "../event/MovementEvents.h"
#include "../event/SpawnEvents.h"
#include "../world/MapWorld.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>

namespace world_v2
{

enum class NoticeKind : std::uint8_t
{
    Spawned,
    Moved,
    Damaged,
    Died,
};

// One thing that happened this tick, flattened into a shape a network layer
// can switch on.
//
// A single POD covering every kind rather than a variant or a class
// hierarchy: notices are produced and consumed within one tick, in the
// thousands, and the whole point of collecting them into a flat vector is
// that neither producing nor filtering them allocates. Unused fields for a
// given kind are simply zero.
//
// Every notice carries its own coordinates. Filtering happens after the
// barrier, by which point the subject may have been despawned -- a death is
// the obvious case -- so a notice that had to look its position up would be
// unfilterable exactly when it matters.
struct Notice
{
    NoticeKind kind = NoticeKind::Spawned;

    // What the notice is about, and who did it -- an attacker or a killer,
    // kNullEntity when nobody did.
    Entity subject = kNullEntity;
    Entity actor = kNullEntity;

    // Where it happened. For a move, the destination.
    int x = 0;
    int y = 0;

    // Move origin only.
    int fromX = 0;
    int fromY = 0;

    // Damage only.
    int amount = 0;
    int remaining = 0;

    // Move speed, or a spawn's template id -- kinds that need neither leave
    // them zero.
    float speed = 0.0f;
    std::uint32_t templateId = 0;
};

// Called once per (viewer, notice) pair that passes the visibility test.
// The viewer is an entity; mapping that to a connection is the game layer's
// job, and the reason nothing here knows what a socket is.
using NoticeSink = std::function<void(Entity viewer, const Notice& notice)>;

// Stage 4: turns a tick's events into the set of notices each viewer should
// receive.
//
// Collection happens through the ordinary event pipeline -- Install
// registers listeners, and they run at the barrier alongside every other
// handler. Delivery happens after, when the world has settled, so a viewer
// is never told about a state that was still being revised.
//
// Filtering is a plain pass over viewers against notices. That is O(viewers
// x notices), which is the obvious thing rather than the clever thing: the
// benchmark measures it, and until that number says otherwise a zone index
// would be complexity bought on a guess.
class BroadcastSystem
{
public:
    // Registers the listeners. Call once during setup, on the world this
    // system will deliver for.
    //
    // Listener order does not matter: every event these read carries its
    // own coordinates, so a notice built after the corpse has already been
    // despawned still knows where the death happened.
    void Install(MapWorld& world)
    {
        world.events.Listen<MonsterSpawnedEvent>(
            [this](const MonsterSpawnedEvent& event)
            {
                Notice notice;
                notice.kind = NoticeKind::Spawned;
                notice.subject = event.entity;
                notice.x = event.x;
                notice.y = event.y;
                notice.templateId = event.templateId;
                m_notices.push_back(notice);
            });

        world.events.Listen<EntityMovedEvent>(
            [this](const EntityMovedEvent& event)
            {
                Notice notice;
                notice.kind = NoticeKind::Moved;
                notice.subject = event.entity;
                notice.x = event.toX;
                notice.y = event.toY;
                notice.fromX = event.fromX;
                notice.fromY = event.fromY;
                notice.speed = event.speed;
                m_notices.push_back(notice);
            });

        world.events.Listen<DamageDealtEvent>(
            [this](const DamageDealtEvent& event)
            {
                Notice notice;
                notice.kind = NoticeKind::Damaged;
                notice.subject = event.target;
                notice.actor = event.attacker;
                notice.x = event.x;
                notice.y = event.y;
                notice.amount = event.amount;
                notice.remaining = event.remainingHealth;
                m_notices.push_back(notice);
            });

        world.events.Listen<DeathEvent>(
            [this](const DeathEvent& event)
            {
                Notice notice;
                notice.kind = NoticeKind::Died;
                notice.subject = event.entity;
                notice.actor = event.killer;
                notice.x = event.x;
                notice.y = event.y;
                m_notices.push_back(notice);
            });
    }

    // Hands every viewer the notices it can see, then empties the list.
    //
    // The list is cleared whether or not a sink is set, so a simulation
    // nobody is watching does not accumulate a tick's worth of notices
    // forever.
    void Deliver(Registry& registry, const NoticeSink& sink)
    {
        if (sink && !m_notices.empty())
        {
            registry.view<ViewerComponent, GridPositionComponent>().Each(
                [&](Entity viewer, ViewerComponent& viewerComponent, GridPositionComponent& position)
                {
                    for (const Notice& notice : m_notices)
                    {
                        if (IsVisibleTo(viewer, position, viewerComponent.radius, notice))
                        {
                            sink(viewer, notice);
                        }
                    }
                });
        }

        m_notices.clear();
    }

    // This tick's notices, before delivery. For tests and diagnostics.
    const std::vector<Notice>& Pending() const
    {
        return m_notices;
    }

private:
    static bool IsVisibleTo(Entity viewer, const GridPositionComponent& position, int radius, const Notice& notice)
    {
        // A viewer always hears about itself.
        //
        // Nothing today can reach this branch: every notice kind carries
        // the subject's own position, so a viewer is trivially within any
        // radius of a notice about itself. It stays as a stated guarantee
        // rather than an emergent coincidence -- the first notice kind
        // whose coordinates are somewhere other than its subject (a
        // teleport, a pet dying across the map) would otherwise silently
        // stop reaching the player it concerns.
        //
        // What this does NOT cover: a viewer removed during the barrier is
        // gone from the registry before delivery runs, so it receives
        // nothing at all that tick, including its own death. A game where
        // players should hear that must not despawn the player entity on
        // death -- see the default DeathEvent handler in CombatRules.h,
        // which despawns unconditionally and is meant to be replaced.
        if (notice.subject == viewer)
        {
            return true;
        }

        if (Within(position, radius, notice.x, notice.y))
        {
            return true;
        }

        // A step is visible if either end of it is: something walking out
        // of range still owes the client the move that took it away, or the
        // creature freezes on screen at the edge of vision.
        return notice.kind == NoticeKind::Moved && Within(position, radius, notice.fromX, notice.fromY);
    }

    // Chebyshev, the same metric movement and AI use.
    static bool Within(const GridPositionComponent& position, int radius, int x, int y)
    {
        const int dx = std::abs(position.x - x);
        const int dy = std::abs(position.y - y);
        return (dx > dy ? dx : dy) <= radius;
    }

    std::vector<Notice> m_notices;
};

} // namespace world_v2
