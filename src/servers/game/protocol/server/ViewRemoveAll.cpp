#include "ViewRemoveAll.h"

#include "common/PayloadWriter.h"

namespace
{
    void WriteIdArray(PayloadWriter& writer, const std::vector<std::uint32_t>& ids)
    {
        writer.Write(static_cast<std::uint32_t>(ids.size()));
        for (const auto id : ids)
        {
            writer.Write(id);
        }
    }
} // namespace

void ViewRemoveAll::Serialize(PayloadWriter& writer) const
{
    WriteIdArray(writer, player_ids);
    WriteIdArray(writer, creature_ids);
    WriteIdArray(writer, item_ids);
}
