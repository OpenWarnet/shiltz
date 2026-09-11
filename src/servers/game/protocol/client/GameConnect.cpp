#include "GameConnect.h"

#include "handlers/Session.h"

bool GameConnect::Deserialize(PayloadReader&)
{
    return false; // never built from a packet
}

void GameConnect::Handle(const GameContext& ctx) const
{
    HandleConnect(ctx);
}
