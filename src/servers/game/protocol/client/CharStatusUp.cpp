#include "CharStatusUp.h"

#include "common/PayloadReader.h"

bool CharStatusUp::Deserialize(PayloadReader& reader)
{
    return reader.Read(stat_id) && reader.Read(amount);
}
