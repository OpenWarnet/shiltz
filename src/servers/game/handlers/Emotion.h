#pragma once

#include "GameContext.h"

struct Emotion;
struct Player;

void HandleEmotion(const GameContext& ctx, const Emotion& request, const Player& player);
