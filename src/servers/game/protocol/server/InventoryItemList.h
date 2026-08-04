#pragma once

#include <array>
#include <cstdint>

class PayloadWriter;

struct InventoryItemSlot
{
    std::uint32_t item_id = 0;
    std::uint32_t qty_or_refine = 0;
    std::array<std::uint8_t, 8> tail{};

    void Serialize(PayloadWriter& writer) const;
};

struct InventoryItemList
{
    static constexpr std::size_t kTotalSlots = 256;
    static constexpr std::size_t kBodySize = 4104;

    // Wire slots 0-12 are the paperdoll/equipment slots (see the layout
    // note above); the general inventory bag starts right after, so
    // inventory_slot.slot_index 0 lands on wire slot kBagStartSlot.
    static constexpr std::size_t kBagStartSlot = 13;

    std::uint32_t total_count = 0;
    std::array<InventoryItemSlot, kTotalSlots> slots{};

    void Serialize(PayloadWriter& writer) const;
};
