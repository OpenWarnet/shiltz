#pragma once

#include "GameContext.h"

struct ItemTradeBuy;
struct ItemTradeSell;
struct Player;

void HandleItemTradeBuy(const GameContext& ctx, const ItemTradeBuy& request, Player& player);
void HandleItemTradeSell(const GameContext& ctx, const ItemTradeSell& request, Player& player);
