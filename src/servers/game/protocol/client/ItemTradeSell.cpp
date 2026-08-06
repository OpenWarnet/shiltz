#include "ItemTradeSell.h"

#include "common/PayloadReader.h"

bool ItemTradeSell::Deserialize(PayloadReader& reader)
{
    return reader.Read(slot_id) && reader.Read(count) && reader.Read(instance_id);
}
