#include "PlayStart.h"

#include "handlers/Session.h"

bool PlayStart::Deserialize(PayloadReader&)
{
    return true;
}

void PlayStart::Handle(const GameContext& ctx) const
{
    HandleCgPlayStart(ctx);
}
