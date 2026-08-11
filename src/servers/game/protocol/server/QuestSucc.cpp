#include "QuestSucc.h"

#include "common/PayloadWriter.h"

void QuestSuccItem::Serialize(PayloadWriter& writer) const
{
    writer.Write(inventory_id);
    writer.Write(slot_id);
    writer.Write(item_id);
    writer.Write(qty_or_refine);
    writer.Write(option);
    writer.Write(option2);
    writer.Write(unknown2);
}

void QuestSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(static_cast<std::uint32_t>(items.size()));

    for (const auto& item : items)
    {
        item.Serialize(writer);
    }

    writer.Write(quest_id);
    writer.Write(money);
    writer.Write(fame);
    writer.Write(exp);
    writer.Write(ap);
    writer.Write(hp);
}
