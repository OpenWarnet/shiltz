#include "StoreOpen.h"

#include "common/PayloadReader.h"

bool StoreOpen::Deserialize(PayloadReader& reader)
{
    return reader.ReadString(password, 16);
}
