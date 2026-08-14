#include "CharMoveUpdate.h"

#include "common/PayloadWriter.h"

void CharMoveUpdate::Serialize(PayloadWriter& writer) const
{
    writer.Write(user_instance_id);
    writer.Write(direction);
    writer.Write(x);
    writer.Write(y);
    writer.Write(speed);
    writer.Write(stop_direction);
}
