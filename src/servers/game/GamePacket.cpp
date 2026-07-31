#include "GamePacket.h"
#include "cipher/BlowfishCipher.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <winsock2.h>

bool GamePacket::Deserialize(std::span<const uint8_t> raw, std::span<const uint8_t> key)
{
    // Frame structure:
    // [[totalLength: uint32_t][body: [code: uint32_t][payload: vector<uint8_t>]]]
    // [[cleartext ---------- ][body: [cleartext---- ][blowfish encrypted----- ]]

    if (raw.size() < sizeof(uint32_t))
    {
        std::cout << "Error: Raw buffer is too small to contain length prefix.\n";
        return false;
    }

    uint32_t totalLength = 0;
    std::memcpy(&totalLength, raw.data(), sizeof(uint32_t));

    if (raw.size() < totalLength)
    {
        std::cout << "Error: Raw buffer size (" << raw.size()
                  << ") is smaller than expected length (" << totalLength << ").\n";
        return false;
    }

    std::cout << "Deserializing GamePacket: Length prefix = " << totalLength
              << ", Raw buffer size = " << raw.size() << "\n";

    std::memcpy(&m_code, raw.data() + sizeof(uint32_t), sizeof(uint32_t));
    std::cout << "Deserialized GamePacket: Code = " << m_code << "\n";

    uint32_t payloadLength = totalLength - sizeof(uint32_t) - sizeof(uint32_t);
    std::cout << "Payload length: " << payloadLength << "\n";
    std::vector<uint8_t> m_encrypted_payload(payloadLength);
    std::memcpy(m_encrypted_payload.data(), raw.data() + sizeof(uint32_t) + sizeof(uint32_t),
                payloadLength);

    // Decrypt
    BlowfishCipher cipher(key);
    bool result = cipher.Decrypt(m_encrypted_payload, m_payload);

    return result;
}

std::vector<uint8_t> GamePacket::Serialize(std::span<const uint8_t> key) const
{
    uint32_t bodyLength = sizeof(uint32_t) + static_cast<uint32_t>(m_payload.size());
    std::cout << "Serializing GamePacket: Code = " << m_code
              << ", Payload size = " << m_payload.size() << ", Body length = " << bodyLength
              << "\n";
    uint32_t totalLength = sizeof(uint32_t) + bodyLength;

    // No Encryption from Server -> Client

    std::vector<uint8_t> buffer(totalLength);
    std::memcpy(buffer.data(), &totalLength, sizeof(uint32_t));
    std::memcpy(buffer.data() + sizeof(uint32_t), &m_code, sizeof(uint32_t));
    std::memcpy(buffer.data() + sizeof(uint32_t) + sizeof(uint32_t), m_payload.data(),
                m_payload.size());

    return buffer;
}
