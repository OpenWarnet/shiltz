#include "SetCharacterMap.h"
#include "common/PayloadReader.h"

bool SetCharacterMap::Deserialize(PayloadReader& reader)
{
    return reader.ReadString(char_name, 16) && reader.Read(server_id) && reader.Read(map_id) &&
           reader.Read(loc_x) && reader.Read(loc_y);
}
