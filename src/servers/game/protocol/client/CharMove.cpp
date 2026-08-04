#include "CharMove.h"
#include "common/PayloadReader.h"

bool CharMove::Deserialize(PayloadReader& reader)
{
    return reader.Read(user_id) && reader.Read(x) && reader.Read(y) && reader.Read(direction) &&
           reader.Read(speed);
}
