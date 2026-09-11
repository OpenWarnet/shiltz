#include "GameEnter.h"

#include "common/PayloadReader.h"
#include "handlers/Session.h"

bool GameEnter::Deserialize(PayloadReader& reader)
{
    return reader.Read(session_id) && reader.Read(unknown) && reader.ReadString(char_name, 16) &&
           reader.ReadString(username, 16) && reader.ReadString(password, 16);
}

void GameEnter::Handle(const GameContext& ctx) const
{
    HandleEnter(ctx, *this);
}
