#include "CrtKillInfo.h"

#include "common/PayloadWriter.h"

void CrtKillInfo::Serialize(PayloadWriter& writer) const
{
    writer.Write(exp_gain);
    writer.Write(current_kill_count);
    writer.Write(max_kill_count);
    writer.Write(multiple_kill_count);
}
