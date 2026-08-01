#include "LoginFail.h"

#include "common/PayloadWriter.h"

void LoginFail::Serialize(PayloadWriter& writer) const
{
    writer.Write(reason);
    std::uint32_t reservedGap[9]{};
    writer.Write(reservedGap);
}
