#include "Login.h"

#include "DESCipher.h"
#include "common/PayloadReader.h"

#include <iostream>

bool Login::Deserialize(PayloadReader& reader)
{
    DESCipher cipher;
    bool readStatus = true;
    uint32_t temp = 0;

    readStatus &= reader.Read(temp);
    readStatus &= reader.ReadString(build, 16);

    std::vector<uint8_t> encryptedUsername;
    readStatus &= reader.ReadBytes(encryptedUsername, 16);
    auto decryptedUsername = cipher.DecryptECB(encryptedUsername);
    username.assign(reinterpret_cast<const char*>(decryptedUsername.data()),
        strnlen(reinterpret_cast<const char*>(decryptedUsername.data()), decryptedUsername.size()));

    std::vector<uint8_t> encryptedPassword;
    readStatus &= reader.ReadBytes(encryptedPassword, 16);
    auto decryptedPassword = cipher.DecryptECB(encryptedPassword);
    password.assign(
        reinterpret_cast<const char*>(decryptedPassword.data()),
        strnlen(reinterpret_cast<const char*>(decryptedPassword.data()), decryptedPassword.size()));

    return readStatus;
}
