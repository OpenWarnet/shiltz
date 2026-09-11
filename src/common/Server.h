#pragma once

#include "ConnectionId.h"

#include <winsock2.h>

#include <boost/asio.hpp>

#include <atomic>

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Generic TCP server: owns an io_context and a small fixed pool of threads
// running it, an acceptor doing async_accept, and one Connection object per
// client doing async reads/writes -- so connection count is not coupled to
// thread count. Each Connection accumulates bytes into [length-prefixed]
// frames (a frame can be split across reads, or several can arrive in one
// read burst), calling OnFrame() once per complete frame.
//
// Derive from this per protocol (see LoginServer/GameServer) and override
// OnFrame() -- and OnClientConnected() if the protocol needs to greet a
// client immediately, like the login server's step-0 nonce frame.
class Server
{
public:
    // Strand type shared by connections and the game server's world tick
    // timer -- any_io_executor so it's constructible from either a socket's
    // executor or an io_context's executor.
    using Strand = boost::asio::strand<boost::asio::any_io_executor>;

    Server(uint16_t port, std::string name = "Server", std::size_t ioThreads = 2);
    virtual ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Binds, listens, and blocks running the io_context (on this thread plus
    // ioThreads-1 helper threads) until the process exits. Returns false if
    // setup failed.
    bool Run();

    // Sends a frame to one connected client. Returns false if the client is
    // no longer connected. Safe to call from any thread -- queues onto that
    // connection's strand and returns immediately, never blocking the
    // caller.
    bool SendTo(ConnectionId connection, std::span<const uint8_t> frame);

    // Sends a frame to every currently connected client. Safe to call from
    // any thread.
    void Broadcast(std::span<const uint8_t> frame);

protected:
    // Called once, right after a client is accepted and registered.
    // Default does nothing.
    virtual void OnClientConnected(ConnectionId connection);

    // Called once, right after a client's connection is torn down and
    // deregistered. Default does nothing -- override to release
    // per-connection state.
    virtual void OnClientDisconnected(ConnectionId connection);

    // Called once per complete [len][code][payload] frame drained from a
    // client's stream.
    virtual void OnFrame(ConnectionId connection, std::span<const uint8_t> frame) = 0;

    // GameServer's tick timer schedules itself on this io_context.
    boost::asio::io_context& IoContext() { return m_ioContext; }

private:
    class Connection;

    void DoAccept();
    void RegisterConnection(const std::shared_ptr<Connection>& connection);
    void UnregisterConnection(ConnectionId connection);

    uint16_t m_port;
    std::string m_name;
    std::size_t m_ioThreads;

    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::vector<std::thread> m_ioThreadPool;

    std::atomic<ConnectionId> m_nextConnectionId{1};

    std::mutex m_connectionsMutex;
    std::unordered_map<ConnectionId, std::shared_ptr<Connection>> m_connections;
};
