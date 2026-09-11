#pragma once

#include "GameContext.h"

struct Player;
struct QuestResult;

void HandleQuestResult(const GameContext& ctx, const QuestResult& request, Player& player);
