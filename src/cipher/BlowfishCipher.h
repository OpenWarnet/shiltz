#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

class BlowfishCipher
{
public:
    explicit BlowfishCipher(std::optional<std::span<const uint8_t>> key = std::nullopt);
    ~BlowfishCipher();

    BlowfishCipher(const BlowfishCipher&) = delete;
    BlowfishCipher& operator=(const BlowfishCipher&) = delete;

    BlowfishCipher(BlowfishCipher&&) noexcept = default;
    BlowfishCipher& operator=(BlowfishCipher&&) noexcept = default;

    [[nodiscard]] bool HasKey();

    [[nodiscard]] bool Decrypt(std::span<const uint8_t> ciphertext,
                               std::vector<uint8_t>& outPlaintext);

    [[nodiscard]] std::vector<uint8_t> Encrypt(std::span<const uint8_t> plaintext);

private:
    class BlockCipher;
    std::unique_ptr<BlockCipher> m_blowfish;
};