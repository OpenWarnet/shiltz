#pragma once

#include "GameContext.h"

struct AttackToCreature;
struct Player;

void HandleAttackToCreature(const GameContext& ctx, const AttackToCreature& request,
                            Player& player);
