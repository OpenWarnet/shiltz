#pragma once

#include "GameContext.h"

struct CharSkillUpEx;
struct Player;

void HandleCharSkillUpEx(const GameContext& ctx, const CharSkillUpEx& request, Player& player);
