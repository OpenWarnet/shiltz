#include "LevelUpCheck.h"

#include "common/PayloadReader.h"

bool LevelUpCheck::Deserialize(PayloadReader& reader)
{
    return reader.Read(session_id);
}
