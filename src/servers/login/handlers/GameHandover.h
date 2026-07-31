#pragma once

#include "LoginDispatcher.h"

struct GameConnect;

void HandleClGameserverConnect(const LoginContext& ctx, const GameConnect& request);
