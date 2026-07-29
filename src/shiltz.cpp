#include <iostream>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    // 1. Initialize Winsock
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 2. Create the listening socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // 3. Define the server address (Listening on port 8080)
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 4. Bind socket to IP and Port
    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // 5. Start listening for incoming connections
    listen(listenSocket, SOMAXCONN);
    std::cout << "Listening on port 8080...\n";

    // 6. Accept a client connection (blocks until a client connects)
    // Accept client connection
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
    std::cout << "Client connected!\n";

    // Buffer to store incoming bytes
    char buffer[1024] = { 0 };
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0) {
        std::cout << "Received (" << bytesReceived << " bytes): " << buffer << "\n";
    }

    // Cleanup
    closesocket(clientSocket);
    closesocket(listenSocket);
    WSACleanup();

    return 0;
}