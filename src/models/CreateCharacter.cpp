#include "CreateCharacter.h"

#include "common/PayloadReader.h"

bool CreateCharacter::Deserialize(PayloadReader& reader)
{
    return reader.Read(server_id) && reader.ReadString(char_name, 16) && reader.Read(slot) &&
           reader.Read(map_id) && reader.Read(loc_x) && reader.Read(loc_y) && reader.Read(gender) &&
           reader.Read(stat_str) && reader.Read(stat_int) && reader.Read(stat_dex) &&
           reader.Read(stat_con) && reader.Read(stat_men) && reader.Read(stat_sen) &&
           reader.Read(hairstyle) && reader.Read(job);
}
