#pragma once

#include <cstdint>
#include <string>

class PayloadReader;

struct Login
{
    std::string build;
    std::string username;
    std::string password;

    bool Deserialize(PayloadReader& reader);
};
