#pragma once

#include "GameContext.h"

struct ItemConfirmNpcRequest;
struct Player;

void HandleItemConfirmNpcRequest(const GameContext& ctx, const ItemConfirmNpcRequest& request, Player& player);
