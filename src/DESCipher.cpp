#include "DESCipher.h"
#include <algorithm>

constexpr int DESCipher::HexNibble(char c) {
    return (c >= '0' && c <= '9') ? (c - '0') : (c - 'a' + 10);
}

uint32_t DESCipher::Feistel(uint32_t half, int round) {
    // 1. Expand 32-bit half to 48 bits via E-table and XOR with round key
    uint64_t expanded = 0;
    for (int i = 0; i < 48; ++i) {
        uint32_t bit = (half >> (32 - kE[i])) & 1;

        int bitPos = round * 48 + i;
        int hexIdx = bitPos / 4;
        int bitInNibble = 3 - (bitPos % 4);
        uint32_t keyBit = (HexNibble(kRoundKeysHex[hexIdx]) >> bitInNibble) & 1;

        expanded |= (static_cast<uint64_t>(bit ^ keyBit) << (47 - i));
    }

    // 2. S-box substitution (8 groups of 6 bits -> 4 bits each)
    uint32_t sbits = 0;
    for (int g = 0; g < 8; ++g) {
        uint8_t chunk = (expanded >> (42 - g * 6)) & 0x3F;
        int row = ((chunk >> 5) & 1) * 2 + (chunk & 1);
        int col = (chunk >> 1) & 0x0F;
        int sboxIndex = row * 16 + col;

        int val = HexNibble(kSboxHex[g * 64 + sboxIndex]);
        sbits |= (static_cast<uint32_t>(val) << (28 - g * 4));
    }

    // 3. Permute 32 bits via P-box
    uint32_t pOut = 0;
    for (int i = 0; i < 32; ++i) {
        uint32_t bit = (sbits >> (32 - kP[i])) & 1;
        pOut |= (bit << (31 - i));
    }

    return pOut;
}

std::array<uint8_t, 8> DESCipher::DecryptBlock(const std::array<uint8_t, 8>& block) const {
    uint64_t in = 0;
    for (int i = 0; i < 8; ++i) in = (in << 8) | block[i];

    uint64_t ip = 0;
    for (int i = 0; i < 64; ++i) {
        ip |= ((in >> (64 - kIP[i])) & 1ULL) << (63 - i);
    }

    uint32_t R = static_cast<uint32_t>(ip >> 32);
    uint32_t L = static_cast<uint32_t>(ip);

    for (int round = 0; round < 16; ++round) {
        uint32_t f = Feistel(R, round);
        uint32_t newR = L ^ f;
        L = R;
        R = newR;
    }

    uint64_t combined = (static_cast<uint64_t>(R) << 32) | L;

    uint64_t fp = 0;
    for (int i = 0; i < 64; ++i) {
        fp |= ((combined >> (63 - (kFP[i] - 1))) & 1ULL) << (63 - i);
    }

    std::array<uint8_t, 8> out{};
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<uint8_t>(fp >> (56 - i * 8));
    }
    return out;
}

std::array<uint8_t, 8> DESCipher::EncryptBlock(const std::array<uint8_t, 8>& block) const {
    uint64_t in = 0;
    for (int i = 0; i < 8; ++i) in = (in << 8) | block[i];

    uint64_t ip = 0;
    for (int i = 0; i < 64; ++i) {
        ip |= ((in >> (64 - kIP[i])) & 1ULL) << (63 - i);
    }

    uint32_t R = static_cast<uint32_t>(ip >> 32);
    uint32_t L = static_cast<uint32_t>(ip);

    for (int round = 15; round >= 0; --round) {
        uint32_t f = Feistel(L, round);
        uint32_t newL = R ^ f;
        R = L;
        L = newL;
    }

    uint64_t combined = (static_cast<uint64_t>(R) << 32) | L;

    uint64_t fp = 0;
    for (int i = 0; i < 64; ++i) {
        fp |= ((combined >> (63 - (kFP[i] - 1))) & 1ULL) << (63 - i);
    }

    std::array<uint8_t, 8> out{};
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<uint8_t>(fp >> (56 - i * 8));
    }
    return out;
}

std::vector<uint8_t> DESCipher::EncryptECB(std::span<const uint8_t> plaintext) const {
    if (plaintext.empty() || plaintext.size() % 8 != 0) return {};

    std::vector<uint8_t> out;
    out.reserve(plaintext.size());

    for (size_t offset = 0; offset < plaintext.size(); offset += 8) {
        std::array<uint8_t, 8> block{};
        std::copy_n(plaintext.begin() + offset, 8, block.begin());

        auto cipher = EncryptBlock(block);
        out.insert(out.end(), cipher.begin(), cipher.end());
    }

    return out;
}

std::vector<uint8_t> DESCipher::DecryptECB(std::span<const uint8_t> ciphertext) const {
    if (ciphertext.empty() || ciphertext.size() % 8 != 0) return {};

    std::vector<uint8_t> out;
    out.reserve(ciphertext.size());

    for (size_t offset = 0; offset < ciphertext.size(); offset += 8) {
        std::array<uint8_t, 8> block{};
        std::copy_n(ciphertext.begin() + offset, 8, block.begin());

        auto plain = DecryptBlock(block);
        out.insert(out.end(), plain.begin(), plain.end());
    }

    return out;
}