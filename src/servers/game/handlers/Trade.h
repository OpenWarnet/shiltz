#pragma once

#include "GameDispatcher.h"

struct ItemTradeBuy;
struct ItemTradeSell;

void HandleItemTradeBuy(const GameContext& ctx, const ItemTradeBuy& request);
void HandleItemTradeSell(const GameContext& ctx, const ItemTradeSell& request);
