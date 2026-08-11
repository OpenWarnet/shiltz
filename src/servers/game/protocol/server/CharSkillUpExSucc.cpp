#include "CharSkillUpExSucc.h"

#include "common/PayloadWriter.h"

void CharSkillUpExSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(remaining_sp);
    writer.Write(remaining_ep);
}
