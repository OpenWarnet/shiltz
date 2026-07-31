#include "BlowfishCipher.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cryptopp/blowfish.h>
#include <cryptopp/crc.h>
#include <cryptopp/filters.h>
#include <cryptopp/misc.h>
#include <cryptopp/modes.h>
#include <cstring>
#include <iostream>
#include <utility>

namespace
{

uint32_t CalculateCrc32(std::span<const uint8_t> data)
{
    CryptoPP::CRC32 crcCalculator;
    crcCalculator.Update(data.data(), data.size());

    uint32_t crcValue = 0;
    crcCalculator.Final(reinterpret_cast<CryptoPP::byte*>(&crcValue));

#if defined(CRYPTOPP_LITTLE_ENDIAN)
    return CryptoPP::ByteReverse(crcValue);
#else
    return crcValue;
#endif
}

void AppendLE32(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 24));
}

} // namespace

class BlowfishCipher::BlockCipher
{
public:
    explicit BlockCipher(std::span<const uint8_t> key) : m_key(key.begin(), key.end()) {}

    std::vector<uint8_t> CbcDecrypt(std::span<const uint8_t> data) const
    {
        const size_t fullBlocksLen = data.size() - (data.size() % 8);
        if (fullBlocksLen == 0)
        {
            return {};
        }

        std::vector<uint8_t> out(fullBlocksLen);
        const std::array<uint8_t, CryptoPP::Blowfish::BLOCKSIZE> zeroIv{};

        try
        {
            CryptoPP::CBC_Mode<CryptoPP::Blowfish>::Decryption decryptor;
            decryptor.SetKeyWithIV(m_key.data(), m_key.size(), zeroIv.data(), zeroIv.size());

            CryptoPP::ArraySource(data.data(), fullBlocksLen, true,
                                  new CryptoPP::StreamTransformationFilter(
                                      decryptor, new CryptoPP::ArraySink(out.data(), out.size()),
                                      CryptoPP::BlockPaddingSchemeDef::NO_PADDING));
        }
        catch (const CryptoPP::Exception& e)
        {
            std::cerr << "Crypto++ Decrypt Error: " << e.what() << '\n';
            return {};
        }

        return out;
    }

    std::vector<uint8_t> CbcEncrypt(std::span<const uint8_t> data) const
    {
        if (data.empty())
        {
            return {};
        }

        std::vector<uint8_t> out(data.size());
        const std::array<uint8_t, CryptoPP::Blowfish::BLOCKSIZE> zeroIv{};

        try
        {
            CryptoPP::CBC_Mode<CryptoPP::Blowfish>::Encryption encryptor;
            encryptor.SetKeyWithIV(m_key.data(), m_key.size(), zeroIv.data(), zeroIv.size());

            CryptoPP::ArraySource(data.data(), data.size(), true,
                                  new CryptoPP::StreamTransformationFilter(
                                      encryptor, new CryptoPP::ArraySink(out.data(), out.size()),
                                      CryptoPP::BlockPaddingSchemeDef::NO_PADDING));
        }
        catch (const CryptoPP::Exception& e)
        {
            std::cerr << "Crypto++ Encrypt Error: " << e.what() << '\n';
            return {};
        }

        return out;
    }

private:
    std::vector<uint8_t> m_key;
};

BlowfishCipher::BlowfishCipher(std::optional<std::span<const uint8_t>> key)
    : m_blowfish(key.has_value() ? std::make_unique<BlockCipher>(key.value()) : nullptr)
{
}

BlowfishCipher::~BlowfishCipher() = default;

bool BlowfishCipher::HasKey()
{
    return m_blowfish != nullptr;
}

bool BlowfishCipher::Decrypt(std::span<const uint8_t> ciphertext,
                             std::vector<uint8_t>& outPlaintext)
{
    if (!HasKey())
    {
        return false;
    }

    std::vector<uint8_t> decrypted = m_blowfish->CbcDecrypt(ciphertext);

    if (decrypted.size() < 8)
    {
        return false;
    }

    uint32_t crc = 0;
    std::memcpy(&crc, decrypted.data() + 4, sizeof(crc));

    std::span<const uint8_t> wrapped(decrypted.data() + 8, decrypted.size() - 8);
    std::vector<uint8_t> body(wrapped.begin(), wrapped.end());

    if (!wrapped.empty())
    {
        const uint8_t pad = wrapped.back();

        if (pad >= 1 && pad <= 8 && wrapped.size() >= pad)
        {
            const bool validPad = std::all_of(wrapped.end() - pad, wrapped.end(),
                                              [pad](uint8_t b) { return b == pad; });

            if (validPad)
            {
                body.assign(wrapped.begin(), wrapped.end() - pad);
            }
        }
    }

    if (CalculateCrc32(body) != crc)
    {
        std::cout << "CRC is not right.." << "\n";
    }

    outPlaintext = std::move(body);
    return true;
}

std::vector<uint8_t> BlowfishCipher::Encrypt(std::span<const uint8_t> plaintext)
{
    if (!HasKey())
    {
        std::cerr << "BlowfishCipher::Encrypt: no key resolved, sending cleartext\n";
        return std::vector<uint8_t>(plaintext.begin(), plaintext.end());
    }

    const auto timestamp =
        static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count());

    std::vector<uint8_t> wrapped;
    wrapped.reserve(8 + plaintext.size() + 8);

    AppendLE32(wrapped, timestamp);
    AppendLE32(wrapped, CalculateCrc32(plaintext));
    wrapped.insert(wrapped.end(), plaintext.begin(), plaintext.end());

    uint8_t pad = static_cast<uint8_t>(8 - (wrapped.size() % 8));
    if (pad == 0)
    {
        pad = 8;
    }

    wrapped.insert(wrapped.end(), pad, pad);

    return m_blowfish->CbcEncrypt(wrapped);
}