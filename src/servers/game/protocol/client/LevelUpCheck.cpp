#include "LevelUpCheck.h"

#include "common/PayloadReader.h"
#include "handlers/LevelUp.h"

bool LevelUpCheck::Deserialize(PayloadReader& reader)
{
    return reader.Read(session_id);
}

void LevelUpCheck::Handle(const GameContext& ctx) const
{
    HandleLevelUpCheck(ctx, *this);
}
