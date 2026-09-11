#pragma once

#include "GameContext.h"

struct GameEnter;
struct GameExit;

void HandleEnter(const GameContext& ctx, const GameEnter& request);
void HandleCgPlayStart(const GameContext& ctx);
void HandleCgExit(const GameContext& ctx, const GameExit& request);
