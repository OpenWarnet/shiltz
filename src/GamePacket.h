#pragma once
#include <cstdint>
#include <vector>
#include <span>

class BlowfishCipher;

class GamePacket {
private:
    uint32_t m_code = 0;
    std::vector<uint8_t> m_payload;

public:
    GamePacket() = default;
    GamePacket(uint32_t code, std::vector<uint8_t> payload)
        : m_code(code), m_payload(std::move(payload)) {};

    [[nodiscard]] const std::vector<uint8_t>& GetPayload() const { return m_payload; }
    [[nodiscard]] uint32_t GetCode() const { return m_code; }

    [[nodiscard]] std::vector<uint8_t> Serialize(std::span<const uint8_t> key = {}) const;
    bool Deserialize(std::span<const uint8_t> raw, std::span<const uint8_t> key = {});
};