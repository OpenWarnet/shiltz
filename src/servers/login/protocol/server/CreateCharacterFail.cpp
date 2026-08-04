#include "CreateCharacterFail.h"

#include "common/PayloadWriter.h"

void CreateCharacterFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
}
