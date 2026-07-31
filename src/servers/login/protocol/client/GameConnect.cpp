#include "GameConnect.h"
#include "common/PayloadReader.h"

bool GameConnect::Deserialize(PayloadReader& reader)
{
    return reader.Read(server_id) && reader.Read(channel_id) && reader.ReadString(char_name, 16);
}