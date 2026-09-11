#include "StorePwModify.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StorePwModify::Deserialize(PayloadReader& reader)
{
    return reader.ReadString(old_password, 16) && reader.ReadString(new_password, 16);
}

void StorePwModify::Handle(const GameContext& ctx, Player& player) const
{
    HandleStorePwModify(ctx, *this, player);
}
