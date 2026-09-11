#pragma once

#include "repositories/BankRepository.h"
#include "world/Item.h"

#include <cstdint>
#include <optional>
#include <vector>

// A player's opened bank account (bank_accounts row), as last committed.
struct Bank
{
    std::int64_t id = 0;
    std::int64_t money = 0;
    std::vector<BankRepository::SlotItem> items;

    std::optional<Item> GetSlot(std::uint32_t slotId) const
    {
        for (const auto& entry : items)
        {
            if (entry.slot_id == slotId)
                return entry.item;
        }
        return std::nullopt;
    }

    void SetSlot(std::uint32_t slotId, const Item& item)
    {
        for (auto& entry : items)
        {
            if (entry.slot_id == slotId)
            {
                entry.item = item;
                return;
            }
        }
        items.push_back(BankRepository::SlotItem{.slot_id = slotId, .item = item});
    }

    void ClearSlot(std::uint32_t slotId)
    {
        std::erase_if(items, [slotId](const BankRepository::SlotItem& entry) { return entry.slot_id == slotId; });
    }
};
