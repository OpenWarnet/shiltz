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
        //uint8_t hexPayload[] = { 0x07, 0x91, 0x85, 0x9B, 0xE9, 0xA2, 0xC7, 0x4E };
        uint8_t hexPayload[] = { 0x61, 0xd4, 0xdd, 0x5b, 0x6c, 0x27, 0x9d, 0x1e };

        // Send the raw buffer and pass the exact byte length
        send(clientSocket, reinterpret_cast<const char*>(hexPayload), sizeof(hexPayload), 0);

        std::cout << "Sent " << sizeof(hexPayload) << " raw hex bytes x!\n";
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}