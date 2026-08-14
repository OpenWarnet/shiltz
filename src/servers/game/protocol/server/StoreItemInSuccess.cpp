#include "StoreItemInSuccess.h"

#include "common/PayloadWriter.h"

void StoreItemInSuccess::Serialize(PayloadWriter& writer) const
{
    writer.Write(inventory_slot_id);
    writer.Write(inventory_item_id);
    writer.Write(inventory_qty_or_refine);
    writer.Write(inventory_option_bits);

    writer.Write(bank_slot_id);
    writer.Write(bank_item_id);
    writer.Write(bank_qty_or_refine);
    writer.Write(bank_option_bits);
}
