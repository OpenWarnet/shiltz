#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>
#include <winsock2.h>

// Generic TCP server: owns the listening socket and, per client, accumulates
// bytes into [length-prefixed] frames (a frame can be split across recv()
// calls, or several can arrive in one recv() burst), calling OnFrame() once
// per complete frame. Each accepted client is serviced on its own thread so
// one slow or stuck connection can't block the others.
//
// Derive from this per protocol (see GameServer/LoginServer) and override
// OnFrame() -- and OnClientConnected() if the protocol needs to greet a
// client immediately, like the login server's step-0 nonce frame.
class TCPServer
{
public:
    TCPServer(uint16_t port, std::string name = "TCPServer");
    virtual ~TCPServer();

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    // Binds, listens, and blocks accepting clients. Returns false if setup
    // failed; otherwise runs until the process exits.
    bool Run();

    // Sends a frame to one connected client. Returns false (and logs) on
    // socket error. Safe to call from any thread, including one that isn't
    // servicing that client -- e.g. a future simulation engine reacting to
    // one client's action by notifying another.
    bool SendTo(SOCKET clientSocket, std::span<const uint8_t> frame);

    // Sends a frame to every currently connected client. Safe to call from
    // any thread; this is the hook a simulation engine pushes unsolicited
    // updates (e.g. a monster moving) through, rather than a reply to
    // whichever client happened to send the triggering packet.
    void Broadcast(std::span<const uint8_t> frame);

protected:
    // Called once, right after a client is accepted and registered.
    // Default does nothing.
    virtual void OnClientConnected(SOCKET clientSocket);

    // Called once per complete [len][code][payload] frame drained from a
    // client's stream.
    virtual void OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame) = 0;

private:
    void ServiceClient(SOCKET clientSocket);

    uint16_t m_port;
    std::string m_name;
    SOCKET m_listenSocket = INVALID_SOCKET;

    std::mutex m_clientsMutex;
    std::vector<SOCKET> m_clients;
};
