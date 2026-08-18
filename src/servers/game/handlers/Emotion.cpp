#include "Emotion.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "protocol/client/Emotion.h"
#include "protocol/server/EmotionSucc.h"

void HandleEmotion(const GameContext& ctx, const Emotion& request)
{
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
        return;

    PayloadWriter writer;
    EmotionSucc response{
        .char_instance_id = session->player.instance_id,
        .emotion_id = request.emotion_id,
        .unknown = 0,
    };
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_EMOTION_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
}
