#include "CharSkillUpExFail.h"

#include "common/PayloadWriter.h"

void CharSkillUpExFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
}
