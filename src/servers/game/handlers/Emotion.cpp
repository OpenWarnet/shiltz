#include "Emotion.h"

#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/Server.h"
#include "protocol/client/Emotion.h"
#include "protocol/server/EmotionSucc.h"

void HandleEmotion(const GameContext& ctx, const Emotion& request)
{
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
        return;

    EmotionSucc response;
    response.char_instance_id = session->character.instance_id;
    response.emotion_id = request.emotion_id;
    response.unknown = 0;
    ctx.server.SendTo(ctx.clientSocket, response.Packet().Serialize(ctx.key));
}
