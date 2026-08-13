#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <span>

class DESCipher {
public:
    DESCipher() = default;
    ~DESCipher() = default;

    [[nodiscard]] std::array<uint8_t, 8> EncryptBlock(const std::array<uint8_t, 8>& block) const;
    [[nodiscard]] std::array<uint8_t, 8> DecryptBlock(const std::array<uint8_t, 8>& block) const;

    [[nodiscard]] std::vector<uint8_t> EncryptECB(std::span<const uint8_t> plaintext) const;
    [[nodiscard]] std::vector<uint8_t> DecryptECB(std::span<const uint8_t> ciphertext) const;

private:
    static uint32_t Feistel(uint32_t half, int round);
    static constexpr int HexNibble(char c);

    // Non-standard DES S-box table: S6 and S7 are swapped relative to the standard spec
    static constexpr char kSboxHex[] =
        "e4d12fb83a6c59070f74e2d1a6cb953841e8d62bfc973a50fc8249175b2ea06df18e6b34972dc05a3d47f28ec01a69b"
        "50e7ba4d158c6932fd8a13f42b67c05e9a09e63f51dc7b428d709346a285ecbf1d6498f30b12c5ae71ad069874fe3b52"
        "c7de3069a1285bc4fd8b56f03472c1ae9a690cb7df13e52843f06a1d8945bc72e2c417ab6853fd0e9eb2c47d150fa398"
        "6421bad78f9c5630eb8c71e2d6f09a4534b2ef08d3c975a61d0b7491ae35c2f8614bdc37eaf6805926bd814a7950fe23"
        "cc1af92680d34e75baf427c9561de0b389ef528c3704a1db6432c95fabe17608dd2846fb1a93e50c71fd8a374c56b0e9"
        "27b419ce206adf35821e74a8dfc90356b";

    // Key: 16 rounds x 48-bit subkey (768 bits total)
    static constexpr char kRoundKeysHex[] =
        "3e02c7bfceff827b0bfbfb3daccb68db7ff9ca507adfdfbd0e966cdadffeee3214efdf6d960e36cff5fbb4a0b06fddd"
        "f40dee8f6fd6fd936487ef3eb8a3eadff77cf8f2b20fea79f96483df3efc70c01fcbfe3f53b501c9f7fd7293f51b37fbe";

    static constexpr int kE[48] = {
        32, 1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,  8,  9,  10, 11,
        12, 13, 12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21,
        22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1
    };

    static constexpr int kP[32] = {
        16, 7,  20, 21, 29, 12, 28, 17, 1,  15, 23, 26, 5,  18, 31, 10,
        2,  8,  24, 14, 32, 27, 3,  9,  19, 13, 30, 6,  22, 11, 4,  25
    };

    static constexpr int kIP[64] = {
        58, 50, 42, 34, 26, 18, 10, 2,  60, 52, 44, 36, 28, 20, 12, 4,
        62, 54, 46, 38, 30, 22, 14, 6,  64, 56, 48, 40, 32, 24, 16, 8,
        57, 49, 41, 33, 25, 17, 9,  1,  59, 51, 43, 35, 27, 19, 11, 3,
        61, 53, 45, 37, 29, 21, 13, 5,  63, 55, 47, 39, 31, 23, 15, 7
    };

    static constexpr int kFP[64] = {
        40, 8,  48, 16, 56, 24, 64, 32, 39, 7,  47, 15, 55, 23, 63, 31,
        38, 6,  46, 14, 54, 22, 62, 30, 37, 5,  45, 13, 53, 21, 61, 29,
        36, 4,  44, 12, 52, 20, 60, 28, 35, 3,  43, 11, 51, 19, 59, 27,
        34, 2,  42, 10, 50, 18, 58, 26, 33, 1,  41, 9,  49, 17, 57, 25
    };
};