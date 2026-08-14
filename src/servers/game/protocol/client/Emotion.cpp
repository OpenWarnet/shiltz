#include "Emotion.h"

#include "common/PayloadReader.h"

bool Emotion::Deserialize(PayloadReader& reader)
{
    return reader.Read(emotion_id) && reader.Read(unknown);
}
