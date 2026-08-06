#pragma once

#include "GameDispatcher.h"

struct QuestResult;

void HandleQuestResult(const GameContext& ctx, const QuestResult& request);
