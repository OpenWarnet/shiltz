#include "GameExit.h"

#include "common/PayloadReader.h"
#include "handlers/Session.h"

bool GameExit::Deserialize(PayloadReader& reader)
{
    return reader.Remaining() == 0;
}

void GameExit::Handle(const GameContext& ctx) const
{
    HandleCgExit(ctx);
}
