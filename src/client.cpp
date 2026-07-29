#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
        // Raw byte array defined using hex notation
        uint8_t hexPayload[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF, 0x42 };

        // Send the raw buffer and pass the exact byte length
        send(clientSocket, reinterpret_cast<const char*>(hexPayload), sizeof(hexPayload), 0);

        std::cout << "Sent " << sizeof(hexPayload) << " raw hex bytes!\n";
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}