#include <iostream>
#include <iomanip>
#include <winsock2.h>

#include "DESCipher.h"
#include "LoginPacket.h"
#include "LoginHandler.h"

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

	//////////// TESTING GROUND

	//uint8_t test[] = { 0xD8, 0x28, 0xF8, 0x6C, 0xFD, 0x23, 0x96, 0xFD, 0x7A, 0x9C, 0xC9, 0x69, 0xA6, 0xE2, 0x23, 0x08 };
	//const auto testplain = cipher.DecryptECB(test);
	//std::cout << "Test Decrypted: " << testplain.data() << "\n";

	///////////


	// 5. Start listening for incoming connections
	listen(listenSocket, SOMAXCONN);
	std::cout << "Listening on port 8080...\n";

	// 6. Accept a client connection (blocks until a client connects)
	// Accept client connection
	SOCKET clientSocket = accept(listenSocket, NULL, NULL);
	std::cout << "Client connected!\n";

	send(clientSocket, reinterpret_cast<const char*>(payload), sizeof(payload), 0);

	// Buffer to store incoming bytes

	uint8_t buffer[1024];
	while (true)
	{
		int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);

		if (bytesReceived <= 0)
		{
			// Client disconnected or error
			std::cout << "Client disconnected\n";
			break;
		}

		try
		{
			LoginPacket packet;
			packet.Deserialize(std::span(buffer, bytesReceived), key);

			std::cout << "Received (" << bytesReceived << " bytes, payload " << packet.GetPayload().size() << " bytes)\n";

			LoginHandler handler(clientSocket, key);
			handler.Handle(packet);
		}
		catch (const std::exception& e)
		{
			std::cerr << "Packet error: " << e.what() << "\n";
		}
	}

	// Cleanup
	closesocket(clientSocket);
	closesocket(listenSocket);
	WSACleanup();

	return 0;
}