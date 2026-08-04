#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Character.h"

class PayloadWriter;

struct CharacterSelection
{
    uint32_t server_id = 0;
    uint32_t char_slot_count = 0;

    std::vector<Character> characters;

    void Serialize(PayloadWriter& writer) const;
};