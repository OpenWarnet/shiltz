#include "CharExitSucc.h"

#include "common/PayloadWriter.h"

void CharExitSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(unused);
}
