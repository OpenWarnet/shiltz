#include "cipher/DESCipher.h"
#include "LoginServer.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main()
{
    // TODO: Hardcoded nonce for testing, should be generated randomly
    uint8_t hexPayload[] = {0x61, 0xd4, 0xdd, 0x5b, 0x6c, 0x27, 0x9d, 0x1e};
    uint8_t payload[] = {0x0C, 0x00, 0x00, 0x00, 0x61, 0xd4, 0xdd, 0x5b, 0x6c, 0x27, 0x9d, 0x1e};
    uint8_t cipherin[] = {0x87, 0xA0, 0x4D, 0x1E, 0x40, 0x80, 0xD7, 0x67,
                          0x1C, 0xFC, 0x01, 0x00, 0x80, 0x34, 0x00, 0x34};

    DESCipher cipher;
    const auto testing = cipher.DecryptECB(cipherin);

    for (size_t i = 0; i < testing.size(); i++)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(testing[i])
                  << " ";
        if ((i + 1) % 16 == 0)
            std::cout << '\n';
    }

    const auto plain = cipher.DecryptECB(hexPayload);

    uint32_t value = 0;
    std::memcpy(&value, plain.data() + 4, sizeof(value)); // little-endian read of bytes [4:8)

    int offset = static_cast<int>((value >> 25) & 7);

    if (offset == 0)
    {
        offset = 7;
    }

    int rotation = (offset + 1) % 8;
    std::cout << "Rotation: " << rotation << "\n";

    // "!@#$%&*+"
    std::array<uint8_t, 8> kKeyTable = {0x21, 0x40, 0x23, 0x24, 0x25, 0x26, 0x2a, 0x2b};

    std::array<uint8_t, 4> key{};

    for (size_t i = 0; i < key.size(); ++i)
    {
        key[i] = kKeyTable[(static_cast<size_t>(rotation) + i) % kKeyTable.size()];
    }

    std::cout << "Key: " << std::string_view(reinterpret_cast<const char*>(key.data()), key.size())
              << "\n";

    LoginServer server(8080, key, payload);
    server.Run();

    return 0;
}
