#include "CharacterSelection.h"

#include "common/PayloadWriter.h"

void CharacterSelection::Serialize(PayloadWriter& writer) const
{
    writer.Write(server_id);
    writer.Write(static_cast<uint32_t>(characters.size()));
    writer.Write(char_slot_count);

    for (const auto& character : characters)
    {
        character.Serialize(writer);
    }
}