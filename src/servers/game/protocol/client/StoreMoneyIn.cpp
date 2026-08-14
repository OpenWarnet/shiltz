#include "StoreMoneyIn.h"

#include "common/PayloadReader.h"

bool StoreMoneyIn::Deserialize(PayloadReader& reader)
{
    return reader.Read(amount);
}
