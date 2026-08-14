#include "EmotionSucc.h"

#include "common/PayloadWriter.h"

void EmotionSucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(char_instance_id);
    writer.Write(emotion_id);
    writer.Write(unknown);
}
