#include "CrtMove.h"

#include "common/PayloadWriter.h"

void CrtMove::Serialize(PayloadWriter& writer) const
{
    writer.Write(creature_id);
    writer.Write(x);
    writer.Write(y);
    writer.Write(target_x);
    writer.Write(target_y);
    writer.Write(speed_raw);
}
