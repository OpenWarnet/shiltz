#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <winsock2.h>

// Dev-facing packet capture: writes every decrypted inbound/outbound packet
// to a human-readable log file per server, plus a dedicated log of opcodes
// that had no registered handler -- a ready list of example payloads for
// whichever opcode needs a handler built next.
//
// Init() must be called once at process start (see login.cpp/game.cpp).
// Log* calls are safe from any thread -- each connection is serviced on its
// own thread (see TCPServer::ServiceClient).
namespace PacketCapture
{
    enum class Direction
    {
        Inbound,  // client -> server
        Outbound, // server -> client
    };

    // Opens logs/<serverName>_packets.log and logs/<serverName>_unhandled.log,
    // creating the logs/ directory if needed.
    void Init(const std::string& serverName);

    // Logs a packet that was (or is about to be) routed to a handler.
    void LogHandled(Direction dir, SOCKET clientSocket, uint32_t opcode,
                     std::string_view opcodeName, std::span<const uint8_t> payload);

    // Logs an inbound packet with no registered handler. Written to both the
    // main packet log (tagged UNHANDLED) and the dedicated unhandled log.
    void LogUnhandled(SOCKET clientSocket, uint32_t opcode, std::string_view opcodeName,
                       std::span<const uint8_t> payload);
}
