#include "Server.h"

#include <array>
#include <cstring>
#include <deque>
#include <exception>
#include <iostream>
#include <utility>

using boost::asio::ip::tcp;

// One per accepted client. Owns the socket, a per-connection strand (so
// reads/writes for this client stay strictly ordered even though several
// connections' handlers may run concurrently across the io_context's thread
// pool), a byte accumulator draining complete [len][code][payload] frames,
// and an outbound write queue so Send() never blocks its caller. Kept alive
// by its own pending async handlers (enable_shared_from_this), not by
// Server.
class Server::Connection : public std::enable_shared_from_this<Connection>
{
public:
    Connection(tcp::socket socket, Server& owner)
        : m_socket(std::move(socket)), m_strand(boost::asio::make_strand(m_socket.get_executor())),
          m_owner(owner), m_id(m_socket.native_handle())
    {
    }

    SOCKET Id() const { return m_id; }

    void Start()
    {
        m_owner.OnClientConnected(Id());
        DoRead();
    }

    // Safe to call from any thread; the actual queue push happens on this
    // connection's strand.
    void Send(std::vector<uint8_t> frame)
    {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [this, self, frame = std::move(frame)]() mutable {
            bool writing = !m_writeQueue.empty();
            m_writeQueue.push_back(std::move(frame));
            if (!writing)
                DoWrite();
        });
    }

private:
    void DoRead()
    {
        auto self = shared_from_this();
        m_socket.async_read_some(
            boost::asio::buffer(m_readChunk),
            boost::asio::bind_executor(m_strand, [this, self](boost::system::error_code ec, std::size_t n) {
                HandleRead(ec, n);
            }));
    }

    void HandleRead(boost::system::error_code ec, std::size_t n)
    {
        if (ec)
        {
            std::cout << "Client disconnected\n";
            HandleDisconnect();
            return;
        }

        m_recvBuffer.insert(m_recvBuffer.end(), m_readChunk.data(), m_readChunk.data() + n);
        DrainFrames();
        DoRead();
    }

    void DrainFrames()
    {
        // A single read can contain zero, one, or several frames back-to-
        // back (TCP has no message boundaries), and a frame can just as
        // easily be split across multiple reads -- so incoming bytes
        // accumulate in m_recvBuffer and get drained a full
        // [len][code][payload] frame at a time, never discarded.
        while (m_recvBuffer.size() >= sizeof(uint32_t))
        {
            uint32_t totalLength = 0;
            std::memcpy(&totalLength, m_recvBuffer.data(), sizeof(uint32_t));

            // totalLength includes its own 4-byte length prefix + the
            // 4-byte code, so anything smaller can't be a real frame.
            if (totalLength < sizeof(uint32_t) * 2)
            {
                std::cerr << "Invalid frame length prefix (" << totalLength
                          << "), dropping connection\n";
                m_recvBuffer.clear();
                break;
            }

            if (m_recvBuffer.size() < totalLength)
            {
                // Frame isn't fully here yet -- wait for the next read.
                break;
            }

            try
            {
                m_owner.OnFrame(Id(), std::span<const uint8_t>(m_recvBuffer.data(), totalLength));
            }
            catch (const std::exception& e)
            {
                std::cerr << "Packet error: " << e.what() << "\n";
            }

            m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + totalLength);
        }
    }

    // Only ever called from within the strand (from Send()'s posted lambda,
    // or from HandleWrite() below), so it's safe to touch m_writeQueue here
    // without a separate lock.
    void DoWrite()
    {
        auto self = shared_from_this();
        boost::asio::async_write(
            m_socket, boost::asio::buffer(m_writeQueue.front()),
            boost::asio::bind_executor(m_strand, [this, self](boost::system::error_code ec, std::size_t) {
                HandleWrite(ec);
            }));
    }

    void HandleWrite(boost::system::error_code ec)
    {
        if (ec)
        {
            std::cout << "send() failed: " << ec.message() << "\n";
            m_writeQueue.clear();
            return;
        }

        m_writeQueue.pop_front();
        if (!m_writeQueue.empty())
            DoWrite();
    }

    void HandleDisconnect()
    {
        // Deregister before closing: a closed SOCKET value can be reused by
        // a later accept(), so the registry entry must be gone first.
        m_owner.UnregisterConnection(Id());
        m_owner.OnClientDisconnected(Id());

        boost::system::error_code ec;
        m_socket.close(ec);
    }

    tcp::socket m_socket;
    Strand m_strand;
    Server& m_owner;
    SOCKET m_id;

    std::array<uint8_t, 1024> m_readChunk{};
    std::vector<uint8_t> m_recvBuffer;
    std::deque<std::vector<uint8_t>> m_writeQueue;
};

Server::Server(uint16_t port, std::string name, std::size_t ioThreads)
    : m_port(port), m_name(std::move(name)), m_ioThreads(ioThreads == 0 ? 1 : ioThreads),
      m_acceptor(m_ioContext)
{
}

Server::~Server()
{
    boost::system::error_code ec;
    m_acceptor.close(ec);
    m_ioContext.stop();

    for (auto& t : m_ioThreadPool)
    {
        if (t.joinable())
            t.join();
    }
}

void Server::OnClientConnected(SOCKET)
{
}

void Server::OnClientDisconnected(SOCKET)
{
}

bool Server::Run()
{
    boost::system::error_code ec;

    tcp::endpoint endpoint(tcp::v4(), m_port);

    m_acceptor.open(endpoint.protocol(), ec);
    if (ec)
    {
        std::cout << "open() failed: " << ec.message() << "\n";
        return false;
    }

    m_acceptor.bind(endpoint, ec);
    if (ec)
    {
        std::cout << "bind() failed: " << ec.message()
                   << " (port already in use by another process/instance?)\n";
        return false;
    }

    m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        std::cout << "listen() failed: " << ec.message() << "\n";
        return false;
    }

    std::cout << m_name << " listening on port " << m_port << "...\n";

    DoAccept();

    // ioThreads-1 helper threads plus the calling thread all drive the same
    // io_context -- Run() blocks until the process exits.
    for (std::size_t i = 1; i < m_ioThreads; ++i)
        m_ioThreadPool.emplace_back([this] { m_ioContext.run(); });

    m_ioContext.run();

    return true;
}

void Server::DoAccept()
{
    m_acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec)
        {
            std::cout << "Client connected!\n";
            auto connection = std::make_shared<Connection>(std::move(socket), *this);
            RegisterConnection(connection);
            connection->Start();
        }
        else
        {
            std::cout << "accept() failed: " << ec.message() << "\n";
        }

        DoAccept();
    });
}

void Server::RegisterConnection(const std::shared_ptr<Connection>& connection)
{
    std::lock_guard lock(m_connectionsMutex);
    m_connections[connection->Id()] = connection;
}

void Server::UnregisterConnection(SOCKET clientId)
{
    std::lock_guard lock(m_connectionsMutex);
    m_connections.erase(clientId);
}

bool Server::SendTo(SOCKET clientId, std::span<const uint8_t> frame)
{
    std::shared_ptr<Connection> connection;
    {
        std::lock_guard lock(m_connectionsMutex);
        auto it = m_connections.find(clientId);
        if (it == m_connections.end())
            return false;
        connection = it->second;
    }

    connection->Send(std::vector<uint8_t>(frame.begin(), frame.end()));
    return true;
}

bool Server::IsConnected(SOCKET clientId) const
{
    std::lock_guard lock(m_connectionsMutex);
    return m_connections.contains(clientId);
}

void Server::Broadcast(std::span<const uint8_t> frame)
{
    // Snapshot shared_ptrs under lock, then send outside it -- a slow
    // client must not stall the registry or delivery to everyone else.
    // Send() never blocks the calling thread at all, regardless of pool
    // size.
    std::vector<std::shared_ptr<Connection>> targets;
    {
        std::lock_guard lock(m_connectionsMutex);
        targets.reserve(m_connections.size());
        for (auto& [id, connection] : m_connections)
            targets.push_back(connection);
    }

    for (auto& connection : targets)
        connection->Send(std::vector<uint8_t>(frame.begin(), frame.end()));
}
