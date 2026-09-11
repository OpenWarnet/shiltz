#include "GameExit.h"

#include "common/PayloadReader.h"
#include "handlers/Session.h"

bool GameExit::Deserialize(PayloadReader& reader)
{
    return reader.Read(opcode_echo);
}

void GameExit::Handle(const GameContext& ctx) const
{
    HandleCgExit(ctx, *this);
}
