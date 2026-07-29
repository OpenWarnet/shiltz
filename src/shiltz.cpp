#include <iostream>
#include <iomanip>
#include <winsock2.h>

#include "DESCipher.h"
#include "LoginPacket.h"

#pragma comment(lib, "ws2_32.lib")

int main() {
	// TODO: Hardcoded nonce for testing, should be generated randomly
	uint8_t hexPayload[] = { 0x61, 0xd4, 0xdd, 0x5b, 0x6c, 0x27, 0x9d, 0x1e };
	uint8_t payload[] = { 0x0C, 0x00, 0x00, 0x00, 0x61, 0xd4, 0xdd, 0x5b, 0x6c, 0x27, 0x9d, 0x1e };

	DESCipher cipher;
	const auto plain = cipher.DecryptECB(hexPayload);
	uint32_t value = 0;
	std::memcpy(&value, plain.data() + 4, sizeof(value)); // little-endian read of bytes [4:8)

	int offset = static_cast<int>((value >> 25) & 7);

	if (offset == 0)
	{
		offset = 7;
	}

	int rotation = (offset + 1) % 8;
	std::cout << "Rotation: " << rotation << "\n";

	// "!@#$%&*+"
	std::array<uint8_t, 8> kKeyTable = { 0x21, 0x40, 0x23, 0x24, 0x25, 0x26, 0x2a, 0x2b };

	std::array<uint8_t, 4> key{};

	for (size_t i = 0; i < key.size(); ++i)
	{
		key[i] = kKeyTable[(static_cast<size_t>(rotation) + i) % kKeyTable.size()];
	}

	std::cout << "Key: " << std::string_view(reinterpret_cast<const char*>(key.data()), key.size()) << "\n";

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

	send(clientSocket, reinterpret_cast<const char*>(payload), sizeof(payload), 0);

	// Buffer to store incoming bytes
	uint8_t buffer[1024] = { 0 };
	int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(buffer), sizeof(buffer) - 1, 0);

	if (bytesReceived > 0) {
		LoginPacket packet;
		packet.Deserialize(std::span(buffer, bytesReceived), key);
		
		std::cout << "Received (" << bytesReceived << " bytes, with " << packet.GetPayload().size() << " bytes payload)\n";

		for (uint8_t byte : packet.GetPayload()) {
			std::cout << std::hex << std::uppercase
				<< std::setw(2) << std::setfill('0')
				<< static_cast<int>(byte) << " ";
		}
		std::cout << std::dec << "\n";
	}

	// Cleanup
	closesocket(clientSocket);
	closesocket(listenSocket);
	WSACleanup();

	return 0;
}