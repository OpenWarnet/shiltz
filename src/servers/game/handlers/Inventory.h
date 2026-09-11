#pragma once

#include "GameContext.h"

struct ItemPickup;
struct ItemMove;
struct ItemDrop;
struct ItemDelete;
struct Player;

void HandleItemPickup(const GameContext& ctx, const ItemPickup& request, Player& player);
void HandleItemMove(const GameContext& ctx, const ItemMove& request, Player& player);
void HandleItemDrop(const GameContext& ctx, const ItemDrop& request, Player& player);
void HandleItemDelete(const GameContext& ctx, const ItemDelete& request, Player& player);
