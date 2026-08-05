#include "CrtLoad.h"

#include "common/PayloadWriter.h"

void CrtLoadRecord::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
    writer.Write(x);
    writer.Write(y);
    writer.Write(monster_id);
    writer.Write(direction);
    writer.Write(hp);
    writer.Write(reserved);
}

void CrtLoad::Serialize(PayloadWriter& writer) const
{
    writer.Write(static_cast<std::uint32_t>(records.size()));
    for (const auto& record : records)
    {
        record.Serialize(writer);
    }
}
