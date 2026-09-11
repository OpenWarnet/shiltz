#include "CharStatusUp.h"

#include "common/PayloadReader.h"
#include "handlers/CharStatus.h"

bool CharStatusUp::Deserialize(PayloadReader& reader)
{
    return reader.Read(stat_id) && reader.Read(amount);
}

void CharStatusUp::Handle(const GameContext& ctx, Player& player) const
{
    HandleCharStatusUp(ctx, *this, player);
}
