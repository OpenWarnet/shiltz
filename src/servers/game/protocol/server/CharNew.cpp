#include "CharNew.h"

void CharNew::Serialize(PayloadWriter& writer) const
{
    record.Serialize(writer, CharOtherLayout::New);
}
