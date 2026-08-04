#include "ItemPickup.h"

#include "common/PayloadReader.h"

bool ItemPickup::Deserialize(PayloadReader& reader)
{
    return reader.Read(id) && reader.Read(slot_id);
}
