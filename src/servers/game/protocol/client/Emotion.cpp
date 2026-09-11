#include "Emotion.h"

#include "common/PayloadReader.h"
#include "handlers/Emotion.h"

bool Emotion::Deserialize(PayloadReader& reader)
{
    return reader.Read(emotion_id) && reader.Read(unknown);
}

void Emotion::Handle(const GameContext& ctx) const
{
    HandleEmotion(ctx, *this);
}
