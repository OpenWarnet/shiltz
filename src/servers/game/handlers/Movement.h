#pragma once

#include "GameContext.h"

struct CharMove;

void HandleMovement(const GameContext& ctx, const CharMove& request);
