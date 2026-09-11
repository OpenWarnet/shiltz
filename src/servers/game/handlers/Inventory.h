#pragma once

#include "GameContext.h"

struct ItemPickup;
struct ItemMove;
struct ItemDrop;
struct ItemDelete;

void HandleItemPickup(const GameContext& ctx, const ItemPickup& request);
void HandleItemMove(const GameContext& ctx, const ItemMove& request);
void HandleItemDrop(const GameContext& ctx, const ItemDrop& request);
void HandleItemDelete(const GameContext& ctx, const ItemDelete& request);
