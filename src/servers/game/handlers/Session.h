#pragma once

#include "GameDispatcher.h"

struct GameEnter;

void HandleEnter(const GameContext& ctx, const GameEnter& request);
void HandleCgPlayStart(const GameContext& ctx);
void HandleCgExit(const GameContext& ctx);
