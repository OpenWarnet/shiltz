#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>
#include <string>

class PayloadWriter;

// GC_SERVER_CHANGE (wire code 0x7CC6E / 511086, s2c) -- tells the client
// which game server/port/session to (re)connect with.
//
// Wire layout confirmed against 14 real captures (OpenShiltz/captures,
// several sessions) -- payload is 36 bytes, not the 32 a naive
// ip+4*uint32 reading would give:
//   server_ip[16] | session_id(u32) | server_type(u32) | channel_id(u32) |
//   unity_insdn_server_type(u32) | server_port(u32)
// `server_port` sits LAST, not right after `server_ip`, and is 1818 in
// every single capture regardless of which server_ip (.172/.173/.177/.178
// all seen) or channel_id (0 in all but one capture, which had 1) --
// matches the same dev game-server port GameConnectSuccess advertises at
// login.
//
// `session_id` is the field right after server_ip -- initially logged as an
// unexplained value that differed every capture, until testing this
// server's own warp flow showed why: the client reconnects after this
// packet by sending a fresh CG_ENTER, and CG_ENTER::session_id must match a
// row in the `session` table (see handlers/Session.cpp's HandleEnter) or
// the reconnect is rejected with GC_ENTER_FAIL. So this field is this
// connection's own GameSession::sessionId -- sending 0 here (as the earlier
// version of this struct did) always fails the reconnect.
struct ServerChange : ServerMessage<GameOpcode::GC_SERVER_CHANGE>
{
    std::string server_ip;
    std::uint32_t session_id = 0;
    std::uint32_t server_type = 0;
    std::uint32_t channel_id = 0;
    std::uint32_t unity_insdn_server_type = 0;
    std::uint32_t server_port = 0;

    void Serialize(PayloadWriter& writer) const override;
};
