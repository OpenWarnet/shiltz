#pragma once

#include "GameContext.h"

struct LevelUpCheck;
struct Player;

void HandleLevelUpCheck(const GameContext& ctx, const LevelUpCheck& request, Player& player);
