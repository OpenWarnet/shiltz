#include "CharOtherLoad.h"

#include "common/PayloadWriter.h"

#include <cstdint>

void CharOtherLoad::Serialize(PayloadWriter& writer) const
{
    writer.Write(static_cast<std::uint32_t>(records.size()));
    for (const auto& record : records)
        record.Serialize(writer, CharOtherLayout::Load);
}
