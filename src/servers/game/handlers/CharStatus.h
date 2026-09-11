#pragma once

#include "GameContext.h"

struct CharStatusUp;
struct Player;

void HandleCharStatusUp(const GameContext& ctx, const CharStatusUp& request, Player& player);
