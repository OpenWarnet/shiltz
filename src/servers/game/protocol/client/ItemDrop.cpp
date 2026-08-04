#include "ItemDrop.h"

#include "common/PayloadReader.h"

bool ItemDrop::Deserialize(PayloadReader& reader)
{
    return reader.Read(slot_id) && reader.Read(quantity);
}
