#pragma once

#include "world/Item.h"

#include <cstdint>

#include "GameDispatcher.h"

struct ItemPickup;
struct ItemMove;
struct ItemDrop;
struct ItemDelete;

void HandleItemPickup(const GameContext& ctx, const ItemPickup& request);

// The other half of a pickup, called from the tick once the simulation has
// decided who got the item. Wired up in GameServer's constructor.
void CompleteItemPickup(const GameContext& ctx, std::uint32_t itemNetworkId, std::uint32_t slotId, const Item& item);
void HandleItemMove(const GameContext& ctx, const ItemMove& request);
void HandleItemDrop(const GameContext& ctx, const ItemDrop& request);
void HandleItemDelete(const GameContext& ctx, const ItemDelete& request);
