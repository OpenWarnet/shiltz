#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class PayloadWriter;

struct BankItemSlot
{
    std::uint32_t item_id = 0;
    std::uint32_t qty_or_refine = 0;
    std::int64_t option_bits = 0;

    void Serialize(PayloadWriter& writer) const;
};

// GC_STORE_OPEN_SUCC (wire code 521113, s2c) -- acknowledges a successful
// CG_STORE_OPEN with the bank's full contents.
struct StoreOpenSucc
{
    static constexpr std::size_t kSlotCount = 80;

    // Layout/meaning not yet identified -- always sent zeroed.
    std::array<std::uint8_t, 36> unknown_header{};

    std::array<BankItemSlot, kSlotCount> slots{};

    // Trailing fields, purpose not yet identified -- always sent zeroed.
    std::uint32_t unknown_a = 0;
    std::uint32_t unknown_b = 0;

    void Serialize(PayloadWriter& writer) const;
};
