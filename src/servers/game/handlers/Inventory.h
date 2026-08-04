#pragma once

#include "GameDispatcher.h"

struct ItemPickup;
struct ItemMove;
struct ItemDrop;

void HandleItemPickup(const GameContext& ctx, const ItemPickup& request);
void HandleItemMove(const GameContext& ctx, const ItemMove& request);
void HandleItemDrop(const GameContext& ctx, const ItemDrop& request);
