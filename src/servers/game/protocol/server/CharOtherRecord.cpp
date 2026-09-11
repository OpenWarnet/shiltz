#include "CharOtherRecord.h"

#include "common/PayloadWriter.h"

void CharOtherRecord::Serialize(PayloadWriter& writer, CharOtherLayout layout) const
{
    writer.Write(unknown_00);
    writer.Write(id);
    writer.WriteString(name, kNameSize);
    writer.Write(x);
    writer.Write(y);
    writer.Write(level);
    writer.Write(job_id);
    writer.Write(gender);
    writer.Write(unknown_2c);
    writer.Write(hairstyle_id);
    writer.Write(face_id);
    writer.Write(max_hp);
    writer.Write(hp);
    writer.Write(unknown_40);

    if (layout == CharOtherLayout::Load)
        writer.Write(direction);

    writer.Write(unknown_4c);

    for (const auto& slot : equipment)
        slot.Serialize(writer);

    writer.WriteString(guild_name, kGuildNameSize);
    writer.Write(guild_state);
    writer.Write(guild_emblem);
    writer.Write(unknown_154);
    writer.Write(unknown_158);

    if (layout == CharOtherLayout::New)
        writer.Write(direction);

    writer.Write(unknown_tail);
}
