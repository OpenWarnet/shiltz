#include "ServerSelect.h"
#include "common/PayloadReader.h"

bool ServerSelect::Deserialize(PayloadReader& reader)
{
    return reader.Read(server_id) && reader.Read(channel_id);
}