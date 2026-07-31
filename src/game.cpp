#include "GameHandler.h"
#include "GamePacket.h"

#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main()
{
    std::array<uint8_t, 28> kBlowfishKey = {
        0x64, 0x6a, 0x78, 0x6f, 0x72, 0x45, 0x6b, 0x64, 0x64, 0x50, 0x74, 0x54, 0x6a, 0x66,
        0x21, 0x40, 0x29, 0x28, 0x21, 0x72, 0x6d, 0x61, 0x6b, 0x73, 0x67, 0x6f, 0x21, 0x00};

    WSADATA wsaData;
    // 1. Initialize Winsock
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 2. Create the listening socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // 3. Define the server address (Listening on port 8080)
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8081);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 4. Bind socket to IP and Port
    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // 5. Start listening for incoming connections
    listen(listenSocket, SOMAXCONN);
    std::cout << "Game Listening on port 8081...\n";

    // 6. Accept a client connection (blocks until a client connects)
    // Accept client connection
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
    std::cout << "Client connected!\n";

    GameHandler handler(clientSocket, kBlowfishKey);

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
            // Client disconnected or error
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
                GamePacket packet;
                packet.Deserialize(std::span(recvBuffer.data(), totalLength), kBlowfishKey);

                std::cout << "Received (" << totalLength << " bytes, payload "
                          << packet.GetPayload().size() << " bytes)\n";

                handler.Handle(packet);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Packet error: " << e.what() << "\n";
            }

            recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + totalLength);
        }
    }

    // Cleanup
    closesocket(clientSocket);
    closesocket(listenSocket);
    WSACleanup();

    return 0;
}