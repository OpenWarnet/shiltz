#include "CharMoveUpdate.h"

#include "common/PayloadWriter.h"

void CharMoveUpdate::Serialize(PayloadWriter& writer) const
{
    writer.Write(user_instance_id);
    writer.Write(x);
    writer.Write(direction);
    writer.Write(y);
    writer.Write(speed);
}
