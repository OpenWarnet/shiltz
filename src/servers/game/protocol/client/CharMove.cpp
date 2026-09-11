#include "CharMove.h"

#include "common/PayloadReader.h"
#include "handlers/Movement.h"

bool CharMove::Deserialize(PayloadReader& reader)
{
    return reader.Read(move_direction) && reader.Read(x) && reader.Read(y) && reader.Read(speed) &&
           reader.Read(stop_direction);
}

void CharMove::Handle(const GameContext& ctx) const
{
    HandleMovement(ctx, *this);
}
