#pragma once

#include "simulation/Components.h"
#include "world_v2/Ids.h"
#include "world_v2/component/Network.h"
#include "world_v2/core/Map.h"
#include "world_v2/core/Module.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace game_sim
{

// The 16x16 zone grid the client's field of view is expressed in, and the
// 3x3 neighbourhood of zones a player can see. Both carried over unchanged
// from the v1 Map so a player sees exactly what they saw before.
inline constexpr int kZoneSize = 16;
inline constexpr int kViewZoneRadius = 1;

// What one viewer is currently told about, and where it last saw each
// thing.
//
// Position is remembered per (viewer, subject) rather than globally,
// because "moved" is a claim about what *this* client has been told, not
// about what happened. A creature that walks while out of view and walks
// back produces one enter, not a move the client could not have followed.
struct WatchedEntity
{
    int x = 0;
    int y = 0;
};

// Turns settled world state into the packets a client is owed.
//
// Why a diff and not an event stream
// ----------------------------------
// The obvious design is to listen for spawn/move/despawn events and send a
// packet per event. It does not work, and the reason is worth writing down
// because it is the whole shape of this file.
//
// GC_CRT_LOAD has to fire when a player walks *towards* a creature that did
// nothing at all. There is no event for that -- the creature has been
// standing there since the map loaded. An event stream can say what
// happened; it cannot say what you can now see. v1 solved this with a
// known_zones delta recomputed on every CG_MOVE, which is a diff wearing a
// different hat.
//
// So this keeps, per viewer, the set of entities that viewer has been told
// about, and each tick compares it against what is visible now:
//
//     entered = visible - known   ->  CrtLoad / ItemMapNew / player load
//     left    = known - visible   ->  ViewRemoveAll
//     moved   = both, moved       ->  CrtMove
//
// That one pass subsumes spawning, despawning, movement and walking into
// range, and it cannot drift: the client's view is recomputed from world
// state every tick rather than accumulated from a stream of edits. It also
// produces exactly the batched shape the wire wants -- CrtLoad carries a
// vector of records and ViewRemoveAll three vectors of ids, and one tick
// per viewer fills them in one go.
//
// Cost is the scan: (3 * kZoneSize)^2 tile probes per viewer per tick,
// against Tile's dense count array rather than its occupant vectors, which
// is the access pattern that array exists for.
//
// Runs at stage 4, after the barrier, so everything it reads has settled.
class ViewModule : public world_v2::Module
{
public:
    // One viewer's worth of changes, already grouped the way the packets
    // are. The sink turns it into wire bytes; this class does no
    // serialization of its own.
    struct ViewDelta
    {
        world_v2::ConnectionId connection = world_v2::kInvalidConnection;

        std::vector<world_v2::Entity> enteredCreatures;
        std::vector<world_v2::Entity> enteredItems;
        std::vector<world_v2::Entity> enteredPlayers;

        std::vector<std::uint32_t> leftCreatureIds;
        std::vector<std::uint32_t> leftItemIds;
        std::vector<std::uint32_t> leftPlayerIds;

        // Network id plus where it moved from and to -- CrtMove wants both
        // ends of the step.
        struct Moved
        {
            std::uint32_t networkId = 0;
            int fromX = 0;
            int fromY = 0;
            int toX = 0;
            int toY = 0;
        };
        std::vector<Moved> moved;

        bool Empty() const
        {
            return enteredCreatures.empty() && enteredItems.empty() && enteredPlayers.empty() &&
                   leftCreatureIds.empty() && leftItemIds.empty() && leftPlayerIds.empty() && moved.empty();
        }
    };

    using ViewSink = std::function<void(world_v2::Map&, const ViewDelta&)>;

    explicit ViewModule(ViewSink sink)
        : m_sink(std::move(sink))
    {
    }

    const char* Name() const override
    {
        return "View";
    }

    void Setup(world_v2::ModuleContext& context) override
    {
        context.AddOutbound([this](world_v2::Map& world) { Publish(world); });
    }

    // Drops a viewer's memory of what it could see.
    //
    // Called when a connection leaves, so a reconnecting client is sent the
    // world afresh rather than inheriting a view built for whoever held the
    // slot before it.
    void Forget(world_v2::Entity viewer)
    {
        m_known.erase(viewer);
    }

private:
    void Publish(world_v2::Map& world)
    {
        // Reap first: a viewer that stopped existing this tick leaves its
        // table behind otherwise, and Entity handles are reused.
        for (auto it = m_known.begin(); it != m_known.end();)
        {
            it = world.registry.Exists(it->first) ? std::next(it) : m_known.erase(it);
        }

        world.registry.view<PlayerIdentityComponent, world_v2::GridPositionComponent>().Each(
            [&](world_v2::Entity viewer, PlayerIdentityComponent&, world_v2::GridPositionComponent& position)
            {
                const auto* session = world.registry.TryGet<world_v2::PlayerSessionComponent>(viewer);
                if (session == nullptr)
                {
                    // A viewer nobody is playing. Visible to the
                    // simulation, not to the wire.
                    return;
                }

                ViewDelta delta;
                delta.connection = session->connection;

                Diff(world, viewer, position, delta);

                if (!delta.Empty() && m_sink)
                {
                    m_sink(world, delta);
                }
            });
    }

    void Diff(world_v2::Map& world, world_v2::Entity viewer, const world_v2::GridPositionComponent& viewerPosition,
              ViewDelta& delta)
    {
        std::unordered_map<world_v2::Entity, WatchedEntity>& known = m_known[viewer];

        m_visible.clear();
        Scan(world, viewer, viewerPosition);

        // entered, and moved
        for (const auto& [entity, now] : m_visible)
        {
            const auto seen = known.find(entity);
            if (seen == known.end())
            {
                Classify(world, entity, delta);
                continue;
            }

            if (seen->second.x != now.x || seen->second.y != now.y)
            {
                if (const auto* network = world.registry.TryGet<NetworkIdComponent>(entity))
                {
                    delta.moved.push_back(
                        ViewDelta::Moved{network->id, seen->second.x, seen->second.y, now.x, now.y});
                }
            }
        }

        // left
        for (const auto& [entity, last] : known)
        {
            if (m_visible.find(entity) != m_visible.end())
            {
                continue;
            }

            // The entity may be gone entirely, so its components cannot be
            // consulted -- which is why the network id is remembered rather
            // than looked up. A despawned creature still has to be taken
            // off the client's screen.
            const auto id = m_networkIds.find(entity);
            if (id == m_networkIds.end())
            {
                continue;
            }

            switch (id->second.kind)
            {
            case ViewKind::Creature:
                delta.leftCreatureIds.push_back(id->second.networkId);
                break;
            case ViewKind::Item:
                delta.leftItemIds.push_back(id->second.networkId);
                break;
            case ViewKind::Player:
                delta.leftPlayerIds.push_back(id->second.networkId);
                break;
            }
        }

        known.clear();
        known.insert(m_visible.begin(), m_visible.end());
    }

    // Everything standing in the 3x3 zone box around the viewer's own zone.
    //
    // Probes Tile's dense per-tile count before touching an occupant list,
    // which is the order that keeps a mostly-empty window cheap -- see
    // Tile.h, where that ordering is worth 40% on the equivalent AI scan.
    void Scan(world_v2::Map& world, world_v2::Entity viewer, const world_v2::GridPositionComponent& viewerPosition)
    {
        const int zoneX = viewerPosition.x / kZoneSize;
        const int zoneY = viewerPosition.y / kZoneSize;

        const int minX = (zoneX - kViewZoneRadius) * kZoneSize;
        const int minY = (zoneY - kViewZoneRadius) * kZoneSize;
        const int maxX = (zoneX + kViewZoneRadius + 1) * kZoneSize - 1;
        const int maxY = (zoneY + kViewZoneRadius + 1) * kZoneSize - 1;

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (world.tiles.OccupantCount(x, y) == 0)
                {
                    continue;
                }

                for (const world_v2::Entity occupant : world.tiles.OccupantsAt(x, y))
                {
                    if (occupant == viewer)
                    {
                        // A client is not told about itself through the
                        // view; its own moves are acknowledged by
                        // GC_CHAR_MOVE instead.
                        continue;
                    }

                    if (!Remember(world, occupant))
                    {
                        continue;
                    }

                    m_visible.emplace(occupant, WatchedEntity{x, y});
                }
            }
        }
    }

    enum class ViewKind
    {
        Creature,
        Item,
        Player,
    };

    struct ViewIdentity
    {
        std::uint32_t networkId = 0;
        ViewKind kind = ViewKind::Creature;
    };

    // Notes what this entity is and what the wire calls it, so the "left"
    // half can name it after it has stopped existing. Returns false for
    // anything the client has no concept of.
    bool Remember(world_v2::Map& world, world_v2::Entity entity)
    {
        const auto* network = world.registry.TryGet<NetworkIdComponent>(entity);
        if (network == nullptr)
        {
            return false;
        }

        ViewKind kind;
        if (world.registry.Has<CreatureComponent>(entity))
        {
            kind = ViewKind::Creature;
        }
        else if (world.registry.Has<MapItemComponent>(entity))
        {
            kind = ViewKind::Item;
        }
        else if (world.registry.Has<PlayerIdentityComponent>(entity))
        {
            kind = ViewKind::Player;
        }
        else
        {
            return false;
        }

        m_networkIds[entity] = ViewIdentity{network->id, kind};
        return true;
    }

    void Classify(world_v2::Map& world, world_v2::Entity entity, ViewDelta& delta)
    {
        if (world.registry.Has<CreatureComponent>(entity))
        {
            delta.enteredCreatures.push_back(entity);
        }
        else if (world.registry.Has<MapItemComponent>(entity))
        {
            delta.enteredItems.push_back(entity);
        }
        else if (world.registry.Has<PlayerIdentityComponent>(entity))
        {
            delta.enteredPlayers.push_back(entity);
        }
    }

    ViewSink m_sink;

    // viewer -> what it has been told about, and where.
    std::unordered_map<world_v2::Entity, std::unordered_map<world_v2::Entity, WatchedEntity>> m_known;

    // Every entity any viewer has ever seen, so a departure can still be
    // named after the entity is gone. Trimmed alongside m_known.
    std::unordered_map<world_v2::Entity, ViewIdentity> m_networkIds;

    // Scratch, reused across viewers so one tick does not allocate per
    // player.
    std::unordered_map<world_v2::Entity, WatchedEntity> m_visible;
};

} // namespace game_sim
