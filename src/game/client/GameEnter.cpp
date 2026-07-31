#include "GameEnter.h"

#include "common/PayloadReader.h"

bool GameEnter::Deserialize(PayloadReader& reader)
{
    return reader.Read(session_id) && reader.Read(unknown) && reader.ReadString(char_name, 16) &&
           reader.ReadString(username, 16) && reader.ReadString(password, 16);
}
