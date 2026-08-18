#pragma once
#include "LoginOpcodes.h"

#include <cstdint>
#include <vector>
#include <span>

class LoginPacket {
private:
    LoginOpcode::Code m_code{};
    std::vector<uint8_t> m_payload;

public:
    LoginPacket() = default;
    LoginPacket(LoginOpcode::Code code, std::vector<uint8_t> payload) : m_code(code), m_payload(std::move(payload)) {};

    [[nodiscard]] const std::vector<uint8_t>& GetPayload() const { return m_payload; }
    [[nodiscard]] LoginOpcode::Code GetCode() const { return m_code; }

    [[nodiscard]] std::vector<uint8_t> Serialize(std::span<const uint8_t> key = {}) const;
    bool Deserialize(std::span<const uint8_t> raw, std::span<const uint8_t> key = {});
};