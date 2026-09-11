#pragma once

#include "GameContext.h"

struct CharMove;
struct Player;

void HandleMovement(const GameContext& ctx, const CharMove& request, Player& player);
