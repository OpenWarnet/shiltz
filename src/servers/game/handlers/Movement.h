#pragma once

#include "GameDispatcher.h"

struct CharMove;

void HandleMovement(const GameContext& ctx, const CharMove& request);
