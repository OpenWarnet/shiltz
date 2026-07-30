#include <iostream>
#include <iomanip>
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
        //uint8_t hexPayload[] = { 0x61, 0xd4, 0xdd, 0x5b, 0x6c, 0x27, 0x9d, 0x1e };
        
        // BC 94 27 2A 24 25 26 2A 16 15 17 12 15 17 16 1F 
        // 24 25 26 2A 43 7A 56 66 07 2D 85 C9 59 6B 2A E3 
        // B2 D8 5C B6 ED 4C 80 C8 35 3E 70 A1 3D F4 7B C1 
        // FB F6 0F 15 30 6A 1D A3 83 3C 8F 96 C2 54 F7 2F 
        // 34 7A 14 AF 06 A2 F4 04
		// CL_LOGIN TEST PACKET
        uint8_t hexPayload[] = {
            0x4C, 0x00, 0x00, 0x00, 
            0xBC, 0x94, 0x27, 0x2A, 0x24, 0x25, 0x26, 0x2A, 0x16, 0x15, 0x17, 0x12, 0x15, 0x17, 0x16, 0x1F,
            0x24, 0x25, 0x26, 0x2A, 0x43, 0x7A, 0x56, 0x66, 0x07, 0x2D, 0x85, 0xC9, 0x59, 0x6B, 0x2A, 0xE3,
            0xB2, 0xD8, 0x5C, 0xB6, 0xED, 0x4C, 0x80, 0xC8, 0x35, 0x3E, 0x70, 0xA1, 0x3D, 0xF4, 0x7B, 0xC1,
            0xFB, 0xF6, 0x0F, 0x15, 0x30, 0x6A, 0x1D, 0xA3, 0x83, 0x3C, 0x8F, 0x96, 0xC2, 0x54, 0xF7, 0X2F, 
            0x34, 0x7A, 0x14, 0xAF, 0x06, 0xA2, 0xF4, 0x04 };

        // CL_GET_CHARINFO
        //uint8_t hexPayload[] = {
        //    0x10, 0x00, 0x00, 0x00,
        //    0xBF, 0x94, 0x27, 0x2A,
        //    0x26, 0x25, 0x26, 0x2A,
        //    0x25, 0x25, 0x26, 0X2A,
        //};

        uint8_t buffer[1024] = { 0 };
        recv(clientSocket, reinterpret_cast<char*>(buffer), sizeof(buffer) - 1, 0);

        // Send the raw buffer and pass the exact byte length
        send(clientSocket, reinterpret_cast<const char*>(hexPayload), sizeof(hexPayload), 0);
        std::cout << "Sent " << sizeof(hexPayload) << " raw hex bytes!\n";

        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(buffer), sizeof(buffer) - 1, 0);

		for (int i = 0; i < bytesReceived; ++i) {
			std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]) << " ";
		}
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}