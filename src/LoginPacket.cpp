#include "LoginPacket.h"
#include "DESCipher.h"
#include <winsock2.h>
#include <cstring>
#include <iostream>

bool LoginPacket::Deserialize(std::span<const uint8_t> raw, std::span<const uint8_t> key) {
    if (raw.size() < sizeof(uint32_t))
    {
		std::cout << "Error: Raw buffer is too small to contain length prefix.\n";
        return false;
    }

    uint32_t length = 0;
    std::memcpy(&length, raw.data(), sizeof(uint32_t));

    if (raw.size() < length) {
		std::cout << "Error: Raw buffer size (" << raw.size() << ") is smaller than expected length (" << length << ").\n";
        return false;
    }

	std::cout << "Deserializing LoginPacket: Length prefix = " << length << ", Raw buffer size = " << raw.size() << "\n";

    m_payload.assign(
        raw.begin() + sizeof(uint32_t),
        raw.begin() + length
    );

    if (m_payload.empty() || key.empty()) return true;

    for (size_t i = 0; i < m_payload.size(); ++i) {
        m_payload[i] ^= key[i % key.size()];
    }

    return true;
}