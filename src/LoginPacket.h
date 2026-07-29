#pragma once
#include <cstdint>
#include <vector>
#include <span>

class DESCipher;

class LoginPacket {
private:
    std::vector<uint8_t> m_payload;

public:
    LoginPacket() = default;

    [[nodiscard]] const std::vector<uint8_t>& GetPayload() const { return m_payload; }

    //[[nodiscard]] std::vector<uint8_t> Serialize() const;
    bool Deserialize(std::span<const uint8_t> raw, std::span<const uint8_t> key = {});
};