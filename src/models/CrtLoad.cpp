#include "CrtLoad.h"

#include "common/PayloadWriter.h"

void CrtLoadRecord::Serialize(PayloadWriter& writer) const
{
    writer.Write(id);
    writer.Write(x);
    writer.Write(y);
    writer.Write(type);
    writer.Write(spawn_count);
    writer.Write(hp);
    writer.Write(f4);
    writer.Write(pos1);
    writer.Write(pos2);
    writer.Write(pos3);
    writer.Write(appearance_raw);
}

void CrtLoad::Serialize(PayloadWriter& writer) const
{
    writer.Write(static_cast<std::uint32_t>(records.size()));
    for (const auto& record : records)
    {
        record.Serialize(writer);
    }
}
