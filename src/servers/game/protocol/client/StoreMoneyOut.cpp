#include "StoreMoneyOut.h"

#include "common/PayloadReader.h"

bool StoreMoneyOut::Deserialize(PayloadReader& reader)
{
    return reader.Read(amount);
}
