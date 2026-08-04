#include "Character.h"

#include "common/PayloadWriter.h"

void CharacterAppearance::Serialize(PayloadWriter& writer) const
{
    writer.Write(gender);
    writer.Write(hairStyle);
    writer.Write(faceStyle);

    writer.Write(headgear);
    writer.Write(headgearRefine);

    writer.Write(top);
    writer.Write(topRefine);

    writer.Write(bottom);
    writer.Write(bottomRefine);

    writer.Write(shoes);
    writer.Write(shoesRefine);

    writer.Write(weapon);
    writer.Write(weaponRefine);

    writer.Write(shield);
    writer.Write(shieldRefine);

    writer.Write(accessory);
    writer.Write(accessoryRefine);

    writer.Write(pet);
    writer.Write(petLevel);

    isDeleted ? writer.Write(deletionInSeconds) : writer.Write(0xFFFFFFFF);

    writer.Write(0x00000000); // Unknown
}

void Character::Serialize(PayloadWriter& writer) const
{
    writer.WriteString(name, 16);

    writer.Write(slot);
    writer.Write(level);
    writer.Write(job);

    appearance.Serialize(writer);

    uint8_t paddingBuffer[256]{};
    writer.Write(paddingBuffer);
}