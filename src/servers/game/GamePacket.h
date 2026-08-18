#pragma once
#include "GameOpcodes.h"

#include <cstdint>
#include <vector>
#include <span>

class BlowfishCipher;

class GamePacket {
private:
    GameOpcode::Code m_code{};
    std::vector<uint8_t> m_payload;

public:
    GamePacket() = default;
    GamePacket(GameOpcode::Code code, std::vector<uint8_t> payload)
        : m_code(code), m_payload(std::move(payload)) {};

    [[nodiscard]] const std::vector<uint8_t>& GetPayload() const { return m_payload; }
    [[nodiscard]] GameOpcode::Code GetCode() const { return m_code; }

    [[nodiscard]] std::vector<uint8_t> Serialize(std::span<const uint8_t> key = {}) const;
    bool Deserialize(std::span<const uint8_t> raw, std::span<const uint8_t> key = {});
};