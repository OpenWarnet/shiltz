#include "LoginPacket.h"
#include "LoginOpcodes.h"
#include "cipher/DESCipher.h"
#include "common/PacketCapture.h"
#include <cstring>
#include <iostream>
#include <iomanip>

bool LoginPacket::Deserialize(std::span<const uint8_t> raw, std::span<const uint8_t> key) {
    // Frame structure:
	// [[totalLength: uint32_t][body: [code: uint32_t][payload: vector<uint8_t>]]]
    // [[cleartext ---------- ][xor encrypted --------------------------------- ]]

    if (raw.size() < sizeof(uint32_t))
    {
		std::cout << "Error: Raw buffer is too small to contain length prefix.\n";
        return false;
    }

    uint32_t totalLength = 0;
    std::memcpy(&totalLength, raw.data(), sizeof(uint32_t));

    if (raw.size() < totalLength) {
		std::cout << "Error: Raw buffer size (" << raw.size() << ") is smaller than expected length (" << totalLength << ").\n";
        return false;
    }

	std::cout << "Deserializing LoginPacket: Length prefix = " << totalLength << ", Raw buffer size = " << raw.size() << "\n";

	uint32_t bodyLength = totalLength - sizeof(uint32_t);
    std::vector<uint8_t> m_body(bodyLength);
    std::memcpy(m_body.data(), raw.data() + sizeof(uint32_t), bodyLength);

    if (!key.empty()) {
        for (size_t i = 0; i < m_body.size(); ++i) {
            m_body[i] ^= key[i % key.size()];
        }
    }

    std::memcpy(&m_code, m_body.data(), sizeof(uint32_t));

    m_payload.assign(
        m_body.begin() + sizeof(uint32_t),
        m_body.begin() + bodyLength
    );

    return true;
}

std::vector<uint8_t> LoginPacket::Serialize(std::span<const uint8_t> key) const {
	uint32_t bodyLength = sizeof(uint32_t) + static_cast<uint32_t>(m_payload.size());
	std::cout << "Serializing LoginPacket: Code = " << static_cast<uint32_t>(m_code) << ", Payload size = " << m_payload.size() << ", Body length = " << bodyLength << "\n";
	uint32_t totalLength = sizeof(uint32_t) + bodyLength;

	std::vector<uint8_t> body(bodyLength);
    std::memcpy(body.data(), &m_code, sizeof(uint32_t));
    std::memcpy(body.data() + sizeof(uint32_t), m_payload.data(), m_payload.size());

    PacketCapture::LogHandled(PacketCapture::Direction::Outbound, static_cast<uint32_t>(m_code),
                              LoginOpcode::ToString(m_code), m_payload);

    if (!key.empty()) {
        for (size_t i = 0; i < body.size(); ++i) {
            body[i] ^= key[i % key.size()];
        }
    }

	std::vector<uint8_t> buffer(totalLength);
	std::memcpy(buffer.data(), &totalLength, sizeof(uint32_t));
	std::memcpy(buffer.data() + sizeof(uint32_t), body.data(), body.size());

	return buffer;
}
