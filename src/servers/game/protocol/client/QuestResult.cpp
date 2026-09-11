#include "QuestResult.h"

#include "common/PayloadReader.h"
#include "handlers/Quest.h"

bool QuestResult::Deserialize(PayloadReader& reader)
{
    return reader.Read(action_id) && reader.Read(creature_instance_id) && reader.Read(unknown);
}

void QuestResult::Handle(const GameContext& ctx, Player& player) const
{
    HandleQuestResult(ctx, *this, player);
}
