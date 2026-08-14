#include "QuestTable.h"

#include <iostream>

namespace
{
bool IsActionable(std::int64_t actionId)
{
    return actionId != 0 && actionId != -1;
}
} // namespace

void QuestTable::Load(const std::filesystem::path& path)
{
    for (QuestActionRecord& record : QuestScr::Load(path))
    {
        if (!IsActionable(record.action_id))
            continue;

        // Known to happen at least once in the shipped data (action_id 1) --
        // last one in file order wins, silently unless logged here. Not
        // necessarily a data bug (the two rows are usually different nodes
        // of the same NPC's tree that happen to share a placeholder id),
        // but worth surfacing since only one definition can ever be live.
        if (m_records.contains(record.action_id))
        {
            std::cout << "QuestTable: action_id " << record.action_id
                      << " redefined -- keeping the later definition\n";
        }

        m_records[record.action_id] = std::move(record);
    }
}

const QuestActionRecord* QuestTable::Find(std::int64_t actionId) const
{
    auto it = m_records.find(actionId);
    return it != m_records.end() ? &it->second : nullptr;
}
