#include "CharSkillUpEx.h"

#include "common/PayloadReader.h"

bool SkillLevelUpEntry::Deserialize(PayloadReader& reader)
{
    return reader.Read(num_level_up) && reader.Read(skill_id);
}

bool CharSkillUpEx::Deserialize(PayloadReader& reader)
{
    std::int32_t count = 0;
    if (!reader.Read(count) || count < 0)
        return false;

    skills.clear();
    skills.reserve(static_cast<std::size_t>(count));

    for (std::int32_t i = 0; i < count; ++i)
    {
        SkillLevelUpEntry entry;
        if (!entry.Deserialize(reader))
            return false;

        skills.push_back(entry);
    }

    std::int32_t terminator = 0;
    return reader.Read(terminator);
}
