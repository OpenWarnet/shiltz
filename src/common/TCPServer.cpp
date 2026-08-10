#include "TCPServer.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

TCPServer::TCPServer(uint16_t port, std::string name) : m_port(port), m_name(std::move(name))
{
}

TCPServer::~TCPServer()
{
    if (m_listenSocket != INVALID_SOCKET)
        closesocket(m_listenSocket);

    WSACleanup();
}

void TCPServer::OnClientConnected(SOCKET)
{
}

void TCPServer::OnClientDisconnected(SOCKET)
{
}

bool TCPServer::Run()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "WSAStartup() failed\n";
        return false;
    }

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
    {
        std::cout << "socket() failed: " << WSAGetLastError() << "\n";
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) ==
        SOCKET_ERROR)
    {
        std::cout << "bind() failed: " << WSAGetLastError()
                   << " (port already in use by another process/instance?)\n";
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cout << "listen() failed: " << WSAGetLastError() << "\n";
        return false;
    }

    std::cout << m_name << " listening on port " << m_port << "...\n";

    while (true)
    {
        SOCKET clientSocket = accept(m_listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cout << "accept() failed: " << WSAGetLastError() << "\n";
            continue;
        }

        std::cout << "Client connected!\n";

        // Detached: each client gets its own recv() loop so a stalled client
        // never blocks the others. Simple first pass -- no thread pool/cap.
        std::thread(&TCPServer::ServiceClient, this, clientSocket).detach();
    }
}

bool TCPServer::SendTo(SOCKET clientSocket, std::span<const uint8_t> frame)
{
    int sent = send(clientSocket, reinterpret_cast<const char*>(frame.data()),
                     static_cast<int>(frame.size()), 0);
    if (sent == SOCKET_ERROR)
    {
        std::cout << "send() failed: " << WSAGetLastError() << "\n";
        return false;
    }

    return true;
}

void TCPServer::Broadcast(std::span<const uint8_t> frame)
{
    // Snapshot under lock, then send() outside it -- a slow/blocked client
    // must not stall the registry (new connects/disconnects) or delivery to
    // everyone else. A client can disconnect between the snapshot and its
    // send(); that just fails and logs here, and ServiceClient deregisters
    // it moments later on its own thread.
    std::vector<SOCKET> targets;
    {
        std::lock_guard lock(m_clientsMutex);
        targets = m_clients;
    }

    for (SOCKET clientSocket : targets)
        SendTo(clientSocket, frame);
}

void TCPServer::ServiceClient(SOCKET clientSocket)
{
    {
        std::lock_guard lock(m_clientsMutex);
        m_clients.push_back(clientSocket);
    }

    OnClientConnected(clientSocket);

    // Bytes read but not yet consumed into a complete frame. A single recv()
    // can contain zero, one, or several frames back-to-back (TCP has no
    // message boundaries), and a frame can just as easily be split across
    // multiple recv() calls -- so incoming bytes accumulate here and get
    // drained a full [len][code][payload] frame at a time, never discarded.
    std::vector<uint8_t> recvBuffer;
    uint8_t readChunk[1024];

    while (true)
    {
        int bytesReceived =
            recv(clientSocket, reinterpret_cast<char*>(readChunk), sizeof(readChunk), 0);

        if (bytesReceived <= 0)
        {
            std::cout << "Client disconnected\n";
            break;
        }

        recvBuffer.insert(recvBuffer.end(), readChunk, readChunk + bytesReceived);

        // Drain every complete frame currently sitting in the buffer.
        while (recvBuffer.size() >= sizeof(uint32_t))
        {
            uint32_t totalLength = 0;
            std::memcpy(&totalLength, recvBuffer.data(), sizeof(uint32_t));

            // totalLength includes its own 4-byte length prefix + the 4-byte
            // code, so anything smaller can't be a real frame.
            if (totalLength < sizeof(uint32_t) * 2)
            {
                std::cerr << "Invalid frame length prefix (" << totalLength
                          << "), dropping connection\n";
                recvBuffer.clear();
                break;
            }

            if (recvBuffer.size() < totalLength)
            {
                // Frame isn't fully here yet -- wait for the next recv().
                break;
            }

            try
            {
                OnFrame(clientSocket, std::span<const uint8_t>(recvBuffer.data(), totalLength));
            }
            catch (const std::exception& e)
            {
                std::cerr << "Packet error: " << e.what() << "\n";
            }

            recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + totalLength);
        }
    }

    {
        std::lock_guard lock(m_clientsMutex);
        std::erase(m_clients, clientSocket);
    }

    OnClientDisconnected(clientSocket);

    closesocket(clientSocket);
}
