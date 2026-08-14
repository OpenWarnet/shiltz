#pragma once

#include "GameDispatcher.h"

struct Emotion;

void HandleEmotion(const GameContext& ctx, const Emotion& request);
