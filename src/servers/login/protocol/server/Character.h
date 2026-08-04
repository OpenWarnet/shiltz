#pragma once

#include <cstdint>
#include <string>

class PayloadWriter;

struct CharacterAppearance
{
    uint32_t gender = 0;
    uint32_t hairStyle = 0;
    uint32_t faceStyle = 0;

    uint32_t headgear = 0;
    uint32_t headgearRefine = 0;

    uint32_t top = 0;
    uint32_t topRefine = 0;

    uint32_t bottom = 0;
    uint32_t bottomRefine = 0;

    uint32_t shoes = 0;
    uint32_t shoesRefine = 0;

    uint32_t weapon = 0;
    uint32_t weaponRefine = 0;

    uint32_t shield = 0;
    uint32_t shieldRefine = 0;

    uint32_t accessory = 0;
    uint32_t accessoryRefine = 0;

    uint32_t pet = 0;
    uint32_t petLevel = 0;

    bool isDeleted = false;
    uint32_t deletionInSeconds = 0;

    void Serialize(PayloadWriter& writer) const;
};

struct Character
{
    std::string name;
    uint32_t slot = 0;
    uint32_t level = 0;
    uint32_t job = 0;

    CharacterAppearance appearance{};

    void Serialize(PayloadWriter& writer) const;
};