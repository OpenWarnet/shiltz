#include "ItemMove.h"

#include "common/PayloadReader.h"

bool ItemMove::Deserialize(PayloadReader& reader)
{
    return reader.Read(source_slot_id) && reader.Read(dest_slot_id);
}
