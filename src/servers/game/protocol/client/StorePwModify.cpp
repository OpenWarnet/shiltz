#include "StorePwModify.h"

#include "common/PayloadReader.h"

bool StorePwModify::Deserialize(PayloadReader& reader)
{
    return reader.ReadString(old_password, 16) && reader.ReadString(new_password, 16);
}
