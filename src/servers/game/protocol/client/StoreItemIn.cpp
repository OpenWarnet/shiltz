#include "StoreItemIn.h"

#include "common/PayloadReader.h"

bool StoreItemIn::Deserialize(PayloadReader& reader)
{
    return reader.Read(inventory_slot_id) && reader.Read(bank_slot_id) && reader.Read(amount);
}
