#include "AttackToCrt.h"

#include "common/PayloadReader.h"

bool AttackToCrt::Deserialize(PayloadReader& reader)
{
    return reader.Read(target_id) && reader.Read(direction) && reader.Read(pos_x) &&
           reader.Read(pos_y) && reader.Read(unknown1) && reader.Read(unknown2);
}
