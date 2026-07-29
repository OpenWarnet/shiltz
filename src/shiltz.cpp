#include <iostream>
#include <winsock2.h>

#include "DESCipher.h"

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

	DESCipher cipher;
	const auto plain = cipher.DecryptECB(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buffer), bytesReceived)
	);
	uint32_t value = 0;
	std::memcpy(&value, plain.data() + 4, sizeof(value)); // little-endian read of bytes [4:8)

	int offset = static_cast<int>((value >> 25) & 7);

	if (offset == 0)
	{
		offset = 7;
	}

	std::cout << "Rotation: " << (offset + 1) % 8 << "\n";

	// "!@#$%&*+"
	std::array<uint8_t, 8> kKeyTable = { 0x21, 0x40, 0x23, 0x24, 0x25, 0x26, 0x2a, 0x2b };

	std::array<uint8_t, 4> key{};

	for (size_t i = 0; i < key.size(); ++i)
	{
		key[i] = kKeyTable[(static_cast<size_t>(offset) + i) % kKeyTable.size()];
	}

	std::cout << "Key: " << std::string_view(reinterpret_cast<const char*>(key.data()), key.size()) << "\n";

	// Cleanup
	closesocket(clientSocket);
	closesocket(listenSocket);
	WSACleanup();

	return 0;
}