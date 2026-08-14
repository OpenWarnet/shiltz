#pragma once

#include <string>

class PayloadReader;

// CG_STORE_CREATE (wire code 411053, c2s) -- request to create the "bank"
// storage feature for this account, presenting the plaintext password to
// provision bank_accounts with (see handlers/Store.cpp).
struct StoreCreate
{
    std::string password;

    bool Deserialize(PayloadReader& reader);
};
