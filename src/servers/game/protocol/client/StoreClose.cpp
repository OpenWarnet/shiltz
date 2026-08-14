#include "StoreClose.h"

#include "common/PayloadReader.h"

bool StoreClose::Deserialize(PayloadReader& reader)
{
    return reader.Read(constant);
}
