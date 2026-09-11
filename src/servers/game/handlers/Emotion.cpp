#include "Emotion.h"

#include "Outbox.h"
#include "protocol/client/Emotion.h"
#include "protocol/server/EmotionSucc.h"
#include "world/Player.h"

void HandleEmotion(const GameContext& ctx, const Emotion& request, const Player& player)
{
    EmotionSucc response;
    response.char_instance_id = player.character.instance_id;
    response.emotion_id = request.emotion_id;
    response.unknown = 0;
    ctx.outbox.Send(ctx.connection, response);
}
