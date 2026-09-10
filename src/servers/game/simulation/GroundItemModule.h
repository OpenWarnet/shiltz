#pragma once

#include "simulation/Components.h"
#include "world/Item.h"
#include "world_v2/core/Map.h"
#include "world_v2/core/Module.h"

#include <cstdint>
#include <functional>

namespace game_sim
{

// Puts an item on the ground. The network id is allocated by the caller,
// because it has to be handed back to the dropping client before the tick
// that creates the entity has run.
struct DropItemCommand
{
    world_v2::ConnectionId connection = world_v2::kInvalidConnection;
    std::uint32_t networkId = 0;
    int x = 0;
    int y = 0;
    Item item;
};

// A player reaching for an item, by the id the client knows it by.
struct PickupCommand
{
    world_v2::ConnectionId connection = world_v2::kInvalidConnection;
    std::uint32_t itemNetworkId = 0;
    std::uint32_t slotId = 0;
};

// An item that has just changed hands. Emitted at the barrier, after the
// claim succeeded and before the item is despawned, so a listener can still
// read what it was.
struct ItemClaimedEvent
{
    world_v2::Entity picker = world_v2::kNullEntity;
    world_v2::ConnectionId connection = world_v2::kInvalidConnection;
    std::uint32_t itemNetworkId = 0;
    std::uint32_t slotId = 0;
    Item item;
};

// Items lying on the ground: dropping them, and claiming them.
//
// Why the claim is at the barrier
// -------------------------------
// Two players can send CG_ITEM_PICKUP for the same item in the same tick.
// The v1 code handled that with a mutex around a flat item list and an
// atomic TryTakeItem; here the whole tick is single-threaded, so the race
// is gone by construction -- both requests drain at stage 1, and the first
// one resolved at the barrier despawns the item, which makes the second
// one's lookup fail. No lock, and no way for both to win.
//
// Reaching for an item is not range-checked yet. Neither was the v1 path --
// it claimed on id alone -- so this is a faithful port rather than a new
// hole, and it is the obvious next thing to tighten now that the server
// knows where everyone actually is.
class GroundItemModule : public world_v2::Module
{
public:
    using ClaimSink = std::function<void(const ItemClaimedEvent&)>;

    explicit GroundItemModule(ClaimSink sink)
        : m_sink(std::move(sink))
    {
    }

    const char* Name() const override
    {
        return "GroundItems";
    }

    void Setup(world_v2::ModuleContext& context) override
    {
        world_v2::Map& world = context.World();

        // Structural, so it happens at stage 1 rather than inside a sweep.
        context.OnCommand<DropItemCommand>(
            [](world_v2::Map& map, const DropItemCommand& command)
            {
                const world_v2::Entity item = map.Spawn(command.x, command.y);
                if (item == world_v2::kNullEntity)
                {
                    // Off the map. The client has already been told the id,
                    // but it will simply never see the item appear, which
                    // is better than an entity nothing can reach.
                    return;
                }

                map.registry.Assign<MapItemComponent>(item, command.item);
                map.registry.Assign<NetworkIdComponent>(item, command.networkId);
            });

        // Recorded as a request rather than resolved here, so the claim
        // happens at the barrier where despawning is legal.
        context.OnPlayerCommand<PickupCommand>(
            [](world_v2::Map& map, world_v2::Entity actor, const PickupCommand& command)
            {
                map.registry.Assign<PickupRequestComponent>(actor, command.itemNetworkId, command.slotId);
            });

        context.AddSystem<PickupRequestSystem>();

        context.Listen<ItemClaimedEvent>(
            [this, &world](const ItemClaimedEvent& event)
            {
                // Announced first, so the listener still sees a world where
                // the item exists.
                if (m_sink)
                {
                    m_sink(event);
                }

                // Then the husk. Removing it is structural, which is why it
                // is here at the barrier and not in the sweep.
                world.registry.view<MapItemComponent, NetworkIdComponent>().Each(
                    [&](world_v2::Entity entity, MapItemComponent&, NetworkIdComponent& network)
                    {
                        if (network.id == event.itemNetworkId)
                        {
                            world.Despawn(entity);
                        }
                    });
            });
    }

    // Turns a pending request into a claim, or drops it.
    //
    // Emits rather than despawns: the item is not the entity this view is
    // visiting, so removing it here would reorder a pool out from under the
    // iteration. The barrier does the removal.
    class PickupRequestSystem : public world_v2::ISystem
    {
    public:
        const char* Name() const override
        {
            return "PickupRequestSystem";
        }

        void Run(world_v2::Map& world, float) override
        {
            world.registry.view<PickupRequestComponent, world_v2::PlayerSessionComponent>().Each(
                [&](world_v2::Entity actor, PickupRequestComponent& request,
                    world_v2::PlayerSessionComponent& session)
                {
                    const std::uint32_t wanted = request.itemNetworkId;
                    const std::uint32_t slotId = request.slotId;
                    const world_v2::ConnectionId connection = session.connection;

                    // One request buys one decision, so a reach for an item
                    // somebody else already took cannot re-fire forever.
                    world.registry.Remove<PickupRequestComponent>(actor);

                    world.registry.view<MapItemComponent, NetworkIdComponent>().Each(
                        [&](world_v2::Entity item, MapItemComponent& mapItem, NetworkIdComponent& network)
                        {
                            if (network.id != wanted)
                            {
                                return;
                            }

                            world.events.Emit(ItemClaimedEvent{actor, connection, wanted, slotId, mapItem.item});
                        });
                });
        }
    };

private:
    ClaimSink m_sink;
};

} // namespace game_sim
