#include "GenericCharacterPayload.h"

#include "common/PayloadReader.h"
#include "common/PayloadWriter.h"


void GenericCharacterPayload::Serialize(PayloadWriter& writer) const
{
    writer.Write(server_id);
    writer.WriteString(char_name, 16);
}

bool GenericCharacterPayload::Deserialize(PayloadReader& reader)
{
    return reader.Read(server_id) && reader.ReadString(char_name, 16);
}
