#include "ItemDelete.h"

#include "common/PayloadReader.h"

bool ItemDelete::Deserialize(PayloadReader& reader)
{
    std::uint32_t confirmFlag = 0;
    std::uint32_t padding = 0;
    return reader.Read(slot_id) && reader.Read(confirmFlag) && reader.Read(padding);
}
