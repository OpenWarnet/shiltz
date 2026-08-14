#include "CharMove.h"

#include "common/PayloadReader.h"

bool CharMove::Deserialize(PayloadReader& reader)
{
    return reader.Read(move_direction) && reader.Read(x) && reader.Read(y) && reader.Read(speed) &&
           reader.Read(stop_direction);
}
