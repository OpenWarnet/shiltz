#include "StoreCreate.h"

#include "common/PayloadReader.h"

bool StoreCreate::Deserialize(PayloadReader& reader)
{
    return reader.ReadString(password, 16);
}
