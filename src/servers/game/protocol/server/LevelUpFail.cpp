#include "LevelUpFail.h"

#include "common/PayloadWriter.h"

void LevelUpFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(level);
    writer.Write(exp);
}
