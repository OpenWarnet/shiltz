#pragma once

#include "protocol/server/CharMoveUpdate.h"
#include "simulation/Components.h"
#include "world_v2/Simulation.h"
#include "world_v2/core/Map.h"
#include "world_v2/core/Module.h"
#include "world_v2/core/System.h"
#include "world_v2/event/MovementEvents.h"

#include <cmath>
#include <cstdint>
#include <functional>

// The game layer's movement rules, as a world_v2 module.
//
// This is the first real packet path through world_v2, and it is
// deliberately the *whole* simulation: no AI, no spawning, no combat, no
// death, no despawn timers, no pickup. A map that only has to move players
// around installs one system, and the tick loop walks a list of one. That
// is what the module system bought.
//
// Why not GridMovementSystem
// --------------------------
// world_v2's own movement system is one-tile stepping: MoveIntentComponent
// carries a direction in [-1, 1] and a step is gated by a per-tile
// cooldown. Seal's client does not work that way -- CG_MOVE names an
// absolute destination and the client has already started walking there.
// Feeding that through a direction-stepper would need server-side pathing
// and would fight the client every step.
//
// So this replaces it rather than wrapping it. Same authority, different
// shape: a destination is checked against the map's bounds and against how
// far the player could possibly have travelled since the last tick, then
// committed through Tile::Move so the position and the occupancy index
// cannot disagree.
namespace game_sim
{

// A CG_MOVE that has been read off the wire and is waiting for a tick.
//
// Carries the connection rather than an Entity for the reason
// Simulation.h's PlayerCommand concept describes: the socket thread cannot
// safely resolve the sender, because the table that answers is being
// rewritten by joins and leaves while the packet is in flight.
struct PlayerMoveCommand
{
    world_v2::ConnectionId connection = world_v2::kInvalidConnection;
    int targetX = 0;
    int targetY = 0;
    std::uint32_t direction = 0;
    std::uint32_t stopDirection = 0;
};

// Carries a joining player's movement stat to the entity the JoinCommand
// is about to create.
//
// A command of its own rather than a field on JoinCommand, because
// JoinCommand belongs to world_v2 and movement_speed is a Seal stat. Pushed
// straight after the join so it drains in the same batch, by which point
// the entity exists.
struct PlayerSpeedCommand
{
    world_v2::ConnectionId connection = world_v2::kInvalidConnection;
    float unitsPerSecond = 0.0f;
};

// Resolves each pending destination against the map, authoritatively.
//
// What it can actually check today
// --------------------------------
// Bounds, and a travel budget. Not terrain -- there is no collision data
// anywhere in the server: map.scr carries no dimensions or walkability, and
// the v1 Map only knows about zones, creatures, items and players. So the
// Tile grid is created fully walkable and Tile::Move refuses a step only at
// the edge of the map.
//
// That is a real limit and worth stating plainly: this stops a client
// claiming a position off the map or teleporting across it, and does not
// stop one walking through a wall. Walls need collision data that does not
// exist yet; when it does, it loads into Tile::SetWalkable and this system
// gains that check for free, because Tile::Move already enforces it.
class PlayerMoveSystem : public world_v2::ISystem
{
public:
    // How much further than the strict budget a move may travel before it
    // is clamped.
    //
    // Generous on purpose. The mapping from movement_speed to map units per
    // second is not verified against a real client, and a tolerance that is
    // too tight would rubber-band ordinary walking. It is a guard against
    // teleports, not a precise physics check -- tighten it once the unit
    // scale is confirmed against a capture.
    float slackMultiplier = 3.0f;

    // Applies when a player has no speed component, so a missing stat
    // cannot silently pin someone in place.
    float fallbackUnitsPerSecond = 64.0f;

    const char* Name() const override
    {
        return "PlayerMoveSystem";
    }

    void Run(world_v2::Map& world, float deltaSeconds) override
    {
        Update(world.registry, world.tiles, world.events, deltaSeconds);
    }

    void Update(world_v2::Registry& registry, world_v2::Tile& tiles, world_v2::EventManager& events,
                float deltaSeconds)
    {
        registry.view<world_v2::GridPositionComponent, MoveRequestComponent>().Each(
            [&](world_v2::Entity entity, world_v2::GridPositionComponent& position, MoveRequestComponent& request)
            {
                // Read before consuming -- removing the component below
                // leaves `request` dangling.
                const int fromX = position.x;
                const int fromY = position.y;
                int toX = request.targetX;
                int toY = request.targetY;
                const std::uint32_t direction = request.direction;

                // Safe here: this is the entity the view is currently
                // visiting, and it is a different pool from `position`.
                registry.Remove<MoveRequestComponent>(entity);

                if (auto* identity = registry.TryGet<PlayerIdentityComponent>(entity))
                {
                    // Facing follows the request even when the step is
                    // clamped -- turning on the spot is always legal.
                    identity->facing = direction;
                }

                ClampToBudget(registry, entity, fromX, fromY, toX, toY, deltaSeconds);

                if (toX == fromX && toY == fromY)
                {
                    // Still emitted: the client is owed an answer to every
                    // CG_MOVE, and "you are where you already were" is the
                    // answer that snaps a desynced client back.
                    events.Emit(world_v2::EntityMovedEvent{entity, fromX, fromY, fromX, fromY, 0.0f});
                    return;
                }

                // Commits the position and the occupancy index together, or
                // neither. Refuses only at the map edge while every tile is
                // walkable -- see the class comment.
                if (!tiles.Move(entity, fromX, fromY, toX, toY))
                {
                    events.Emit(world_v2::EntityMovedEvent{entity, fromX, fromY, fromX, fromY, 0.0f});
                    return;
                }

                position.x = toX;
                position.y = toY;

                events.Emit(world_v2::EntityMovedEvent{entity, fromX, fromY, toX, toY, 0.0f});
            });
    }

private:
    // Pulls the destination back along the line from where the player
    // actually is, if it is further than they could have travelled.
    //
    // Clamped rather than rejected: a client that is slightly ahead of the
    // server -- which is every client, always -- should keep moving, just
    // not arrive early. A rejection would rubber-band on ordinary latency.
    void ClampToBudget(world_v2::Registry& registry, world_v2::Entity entity, int fromX, int fromY, int& toX,
                       int& toY, float deltaSeconds) const
    {
        float speed = fallbackUnitsPerSecond;
        if (const auto* component = registry.TryGet<PlayerSpeedComponent>(entity))
        {
            if (component->unitsPerSecond > 0.0f)
            {
                speed = component->unitsPerSecond;
            }
        }

        const float budget = speed * deltaSeconds * slackMultiplier;
        if (budget <= 0.0f)
        {
            toX = fromX;
            toY = fromY;
            return;
        }

        const float deltaX = static_cast<float>(toX - fromX);
        const float deltaY = static_cast<float>(toY - fromY);
        const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);

        if (distance <= budget)
        {
            return;
        }

        const float scale = budget / distance;
        toX = fromX + static_cast<int>(deltaX * scale);
        toY = fromY + static_cast<int>(deltaY * scale);
    }
};

// Everything a movement-only map needs: how a player enters, what CG_MOVE
// does, the system that resolves it, and the packet that goes back.
//
// The acknowledgement is built from EntityMovedEvent rather than from a
// Notice. For this packet that is the right call and worth recording:
// GC_CHAR_MOVE acknowledges the mover's own move, so there is exactly one
// recipient and nothing to filter by visibility. Notice earns its keep on
// the packets that go to *other* players -- a spawn or a creature move seen
// by everyone in range -- which is a different path and not this one.
class PlayerMoveModule : public world_v2::Module
{
public:
    using MoveSink = std::function<void(world_v2::ConnectionId, const CharMoveUpdate&)>;

    explicit PlayerMoveModule(MoveSink sink)
        : m_sink(std::move(sink))
    {
    }

    const char* Name() const override
    {
        return "PlayerMove";
    }

    void Setup(world_v2::ModuleContext& context) override
    {
        world_v2::Map& world = context.World();

        // The JoinCommand carries where the character was last saved. The
        // simulation takes that on trust exactly once, on entry, and is
        // authoritative from then on.
        context.OnSpawnPlayer([](world_v2::Map& map, const world_v2::JoinCommand& command) -> world_v2::Entity {
            const world_v2::Entity player = map.Spawn(command.x, command.y);
            if (player == world_v2::kNullEntity)
            {
                // Off the map. Refusing is right: a character saved at a
                // bad position should fail to enter loudly rather than be
                // silently teleported to the origin.
                return world_v2::kNullEntity;
            }

            map.registry.Assign<PlayerIdentityComponent>(player, command.character, std::uint32_t{0});

            // The id other clients will see this player by. Same value the
            // client itself knows -- CharacterDataLoad::self_entity_id --
            // so a player recognises itself in someone else's view list.
            map.registry.Assign<NetworkIdComponent>(player, command.character);
            return player;
        });

        context.AddSystem<PlayerMoveSystem>();

        context.OnPlayerCommand<PlayerSpeedCommand>(
            [](world_v2::Map& map, world_v2::Entity actor, const PlayerSpeedCommand& command)
            {
                map.registry.Assign<PlayerSpeedComponent>(actor, command.unitsPerSecond);
            });

        context.OnPlayerCommand<PlayerMoveCommand>(
            [](world_v2::Map& map, world_v2::Entity actor, const PlayerMoveCommand& command)
            {
                map.registry.Assign<MoveRequestComponent>(actor, command.targetX, command.targetY, command.direction,
                                                          command.stopDirection);
            });

        // Stage 3. The move has been decided by the time this runs, so the
        // position it reports is the settled one -- which is the whole
        // point: the client is told where it actually is, not what it
        // asked for.
        context.Listen<world_v2::EntityMovedEvent>(
            [this, &world](const world_v2::EntityMovedEvent& event)
            {
                if (!world.registry.Exists(event.entity))
                {
                    return;
                }

                const auto* identity = world.registry.TryGet<PlayerIdentityComponent>(event.entity);
                const auto* session = world.registry.TryGet<world_v2::PlayerSessionComponent>(event.entity);
                if (identity == nullptr || session == nullptr)
                {
                    // A creature moved, not a player. Nothing to
                    // acknowledge -- other players seeing it is a different
                    // packet on a path that does not exist yet.
                    return;
                }

                CharMoveUpdate update{};
                update.user_instance_id = identity->instanceId;
                update.direction = identity->facing;
                update.x = static_cast<std::uint32_t>(event.toX);
                update.y = static_cast<std::uint32_t>(event.toY);
                update.speed = SpeedFor(world, event.entity);
                update.stop_direction = identity->facing;

                if (m_sink)
                {
                    m_sink(session->connection, update);
                }
            });
    }

private:
    // CharMoveUpdate::speed is documented as the player's own derived
    // movement_speed plus 300, deliberately not an echo of CharMove::speed
    // -- see protocol/server/CharMoveUpdate.h.
    static std::uint32_t SpeedFor(world_v2::Map& world, world_v2::Entity entity)
    {
        constexpr std::uint32_t kSpeedBias = 300;

        if (const auto* speed = world.registry.TryGet<PlayerSpeedComponent>(entity))
        {
            return static_cast<std::uint32_t>(speed->unitsPerSecond) + kSpeedBias;
        }

        return kSpeedBias;
    }

    MoveSink m_sink;
};

} // namespace game_sim
