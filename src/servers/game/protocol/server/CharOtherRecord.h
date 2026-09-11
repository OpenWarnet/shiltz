#pragma once

#include "protocol/server/InventoryItemList.h" // InventoryItemSlot, kBagStartSlot

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class PayloadWriter;

// Where `direction` sits: 0x48 in GC_CHAR_OTHER_LOAD, 0x2d4 in GC_CHAR_NEW.
enum class CharOtherLayout : std::uint8_t
{
    Load,
    New,
};

// Another character as GC_CHAR_OTHER_LOAD / GC_CHAR_NEW describe it.
struct CharOtherRecord
{
    static constexpr std::size_t kNameSize = 16;
    static constexpr std::size_t kGuildNameSize = 20;

    static constexpr std::size_t kEquipmentSlots = InventoryItemList::kBagStartSlot;

    std::uint32_t unknown_00 = 0; 
    std::uint32_t id = 0; 
    std::string name;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t level = 0;
    std::uint32_t job_id = 0;
    std::uint32_t gender = 0;
    std::uint32_t unknown_2c = 0;
    std::uint32_t hairstyle_id = 0;
    std::uint32_t face_id = 0;
    std::uint32_t max_hp = 0;
    std::uint32_t hp = 0;
    std::array<std::uint32_t, 2> unknown_40{};
    std::uint32_t direction = 0;
    std::array<std::uint32_t, 7> unknown_4c{};
    std::array<InventoryItemSlot, kEquipmentSlots> equipment{};
    std::string guild_name;
    std::uint32_t guild_state = 0;
    std::uint32_t guild_emblem = 0;
    std::uint32_t unknown_154 = 0;
    // Holds a second item-shaped array (costume/cash appearance)
    std::array<std::uint32_t, 96> unknown_158{};
    std::uint32_t unknown_tail = 0;

    void Serialize(PayloadWriter& writer, CharOtherLayout layout) const;
};
