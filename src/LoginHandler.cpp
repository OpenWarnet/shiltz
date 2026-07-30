#include "LoginHandler.h"
#include "LoginPacket.h"
#include "DESCipher.h"
#include <iostream>
#include <iomanip>

LoginHandler::LoginHandler(SOCKET clientSocket, std::span<const uint8_t> key) :
	m_clientSocket(clientSocket), m_key(key)
{
}

bool LoginHandler::Handle(LoginPacket packet)
{
	if (packet.GetCode() == 111000) // CL_LOGIN
	{
		const auto& payload = packet.GetPayload();
		std::cout << "Received CL_LOGIN packet with payload size: " << payload.size() << "\n";

		char build_version[16] = {};
		std::memcpy(build_version, payload.data() + sizeof(uint32_t), 16);

		std::cout << "Build Version: " << build_version << "\n";

		DESCipher cipher;

		std::vector<uint8_t> encrypted_user_name(16);
		std::memcpy(encrypted_user_name.data(), payload.data() + sizeof(uint32_t) + 16, 16);
		const auto user_name = cipher.DecryptECB(encrypted_user_name);
		std::cout << "Decrypted User Name: " << user_name.data() << "\n";

		std::vector<uint8_t> encrypted_password(16);
		std::memcpy(encrypted_password.data(), payload.data() + sizeof(uint32_t) + 32, 16);
		const auto password = cipher.DecryptECB(encrypted_password);
		std::cout << "Decrypted Password: " << password.data() << "\n";

		// TODO: Somehow it doesn't matter what I sent, this will not change the Server Select UI.
		LoginPacket responsePacket(221001, {
			0x02, 0x00, 0x00, 0x00, // Num of server
			0x31, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
			0x01, 0x00, 0x00, 0x00, // Num of channel
			0x00, 0x00, 0x00, 0x00, // Number of users per channel
		}); // LC_LOGIN_SUCCESS
		auto response = responsePacket.Serialize(m_key);

		send(m_clientSocket, reinterpret_cast<const char*>(response.data()), static_cast<int>(response.size()), 0);

		return true;
	}
	else if (packet.GetCode() == 111050) // CL_USER_SYSTEM_SPEC_INFO
	{
		std::cout << "Received CL_USER_SYSTEM_SPEC_INFO packet.\n";
		return true;
	}
	else if (packet.GetCode() == 111020) // CL_GAMEGUARD
	{
		std::cout << "Received CL_GAMEGUARD packet.\n";
		return true;
	}
	else if (packet.GetCode() == 111003) // CL_GET_CHARINFO
	{
		const auto& payload = packet.GetPayload();
		std::cout << "Received CL_GET_CHARINFO packet with payload size: " << payload.size() << "\n";
		for (size_t i = 0; i < payload.size(); ++i)
		{
			std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(payload[i]) << " ";
		}
		std::cout << "\n";
		std::cout << std::dec;

		uint32_t server_id;
		std::memcpy(&server_id, payload.data(), 4);
		std::cout << "Server ID: " << server_id << "\n";

		uint32_t channel_id;
		std::memcpy(&channel_id, payload.data() + 4, 4);
		std::cout << "Channel ID: " << channel_id << "\n";

		// Send characters
		std::vector<uint8_t> responseBody;
		uint32_t responseLength = 0;
		// Server ID
		responseBody.resize(sizeof(server_id));
		std::memcpy(responseBody.data(), &server_id, sizeof(server_id));
		responseLength += sizeof(server_id);
		
		// Character Count
		uint32_t character_count = 1;
		responseBody.resize(responseBody.size() + sizeof(character_count));
		std::memcpy(responseBody.data() + responseLength, &character_count, sizeof(character_count));
		responseLength += sizeof(character_count);

		// Character Empty Slots Num
		uint32_t empty_slots = 5;
		responseBody.resize(responseBody.size() + sizeof(empty_slots));
		std::memcpy(responseBody.data() + responseLength, &empty_slots, sizeof(empty_slots));
		responseLength += sizeof(empty_slots);

		// Character Info Start
		// Character Name
		std::string name = "Admin";
		uint8_t buffer[16]{};
		std::memcpy(buffer, name.data(), name.size());
		responseBody.resize(responseBody.size() + sizeof(buffer));
		std::memcpy(responseBody.data() + responseLength, buffer, sizeof(buffer));
		responseLength += sizeof(buffer);

		// Character Slot
		uint32_t slot_num = 1;
		responseBody.resize(responseBody.size() + sizeof(slot_num));
		std::memcpy(responseBody.data() + responseLength, &slot_num, sizeof(slot_num));
		responseLength += sizeof(slot_num);

		// Character Level
		uint32_t level = 271;
		responseBody.resize(responseBody.size() + sizeof(level));
		std::memcpy(responseBody.data() + responseLength, &level, sizeof(level));
		responseLength += sizeof(level);

		// Character Job
		uint32_t job = 1;
		responseBody.resize(responseBody.size() + sizeof(job));
		std::memcpy(responseBody.data() + responseLength, &job, sizeof(job));
		responseLength += sizeof(job);

		// Character Appearance
		// 84-bytes or 21 ints
		uint32_t values[21] = {
			1, // Gender
			4, // Hair style
			1, // Face style
			31163, // Headgear
			0, // Headgear Refine
			1004, // Top
			0, // Top Refine
			1005, // Bottom
			0, // Bottom Refine
			1006, // Shoes
			0, // Shoes Refine
			1007, // Weapon
			12, // Weapon Refine
			30547, // Shield
			6, // Shield Refine
			26431, // Accessories
			6, // Accessories Refine
			0, // Pet
			0, // Pet Level?
			0xFFFFFFFF,
			0x00 // ?
		};

		for (uint32_t value : values)
		{
			size_t offset = responseBody.size();
			responseBody.resize(offset + sizeof(value));

			std::memcpy(responseBody.data() + offset, &value, sizeof(value));
			responseLength += sizeof(value);
		}

		// Padding 256-bytes
		uint8_t paddingBuffer[256]{};
		responseBody.resize(responseBody.size() + sizeof(paddingBuffer));
		std::memcpy(responseBody.data() + responseLength, paddingBuffer, sizeof(paddingBuffer));
		responseLength += sizeof(paddingBuffer);

		for (size_t i = 0; i < responseBody.size(); ++i)
		{
			std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(responseBody[i]) << " ";
		}
		std::cout << std::dec;

		LoginPacket responsePacket(221003, responseBody); // LC_CHARINFO_SUCCESS
		auto response = responsePacket.Serialize(m_key);

		send(m_clientSocket, reinterpret_cast<const char*>(response.data()), static_cast<int>(response.size()), 0);

		return true;
	}
	else
	{
		std::cout << "Received unknown packet code: " << std::hex << packet.GetCode() << "\n";
	}
	return false;
}