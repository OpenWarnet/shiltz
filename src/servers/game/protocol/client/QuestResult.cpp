#include "QuestResult.h"

#include "common/PayloadReader.h"

bool QuestResult::Deserialize(PayloadReader& reader)
{
    return reader.Read(action_id) && reader.Read(creature_instance_id) && reader.Read(unknown);
}
