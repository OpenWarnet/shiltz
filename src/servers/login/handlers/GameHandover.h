#pragma once
#include "LoginDispatcher.h"

struct GameConnect;

void HandleGameServerConnection(const LoginContext& ctx, const GameConnect& request);
