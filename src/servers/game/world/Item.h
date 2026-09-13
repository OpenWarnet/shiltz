#pragma once

#include <cstdint>
#include <optional>

struct CharacterDerivedStats;
struct ItemRecord;

// The canonical "an item, in some quantity, possibly refined, possibly
// appraised" payload -- the shape shared by an equipment_slot row, an
// inventory_slot row, and a dropped item (see world/Drop.h).
// Not the wire format (InventoryItemSlot, protocol/server/
// InventoryItemList.h), which packs quantity/refine_level into one
// dual-purpose field; see WireQuantityOrRefine() for that projection.
struct Item
{
    // Sentinel for option_bits meaning "never appraised" -- see
    // handlers/ItemConfirmNpc.cpp, which is the only writer of a real
    // rolled value.
    static constexpr std::uint64_t kNeverAppraised = 0xFFFFFFFFFFFFFFFFu;

    std::uint32_t item_id = 0;
    std::uint32_t quantity = 0;
    std::uint32_t refine_level = 0;
    // Discriminates quantity vs refine_level: set means an equippable item
    // (quantity meaningless), unset means a stackable item (refine_level
    // meaningless). Mirrors the equipment_slot/inventory_slot NULL-ness
    // convention (see db/migrations/sqlite/0011_simplify_item_magic_option_columns.sql).
    bool has_refine_level = false;

    std::int32_t item_level = 0;
    std::uint64_t option_bits = 0;

    // Wire's dual-purpose slot field: refine_level as-is for an equippable,
    // else quantity - 1 for a stackable (a stack of N displays as N, backed
    // by quantity = N - 1 on the wire -- see InventoryItemList.h).
    std::uint32_t WireQuantityOrRefine() const
    {
        if (has_refine_level)
        {
            return refine_level;
        }

        return quantity > 0 ? quantity - 1 : 0;
    }

    // What `slot` becomes after receiving `amount` units of *this* item (used as the source of
    // truth for item_level/option_bits when placing fresh) -- nullopt if slot holds a different
    // item_id or either side is equippable (has_refine_level), which can never stack. Callers
    // decide what "can't" means for them (put a drop back on the ground, fail the request, drop
    // it silently) -- shared by every handler where the client names the target slot itself
    // (CG_ITEM_PICKUP, CG_STORE_ITEM_IN/OUT, CG_ITEM_TRADE_BUY).
    std::optional<Item> StackedInto(const std::optional<Item>& slot, std::uint32_t amount) const
    {
        if (slot)
        {
            if (slot->item_id != item_id || slot->has_refine_level || has_refine_level)
                return std::nullopt;

            Item updated = *slot;
            updated.quantity += amount;
            return updated;
        }

        Item fresh = *this;
        fresh.quantity = amount;
        return fresh;
    }

    // This item's full derived-stat contribution: `record`'s own flat
    // `*_bonus` columns, plus this item's magic-option roll and refine
    // ("+N") growth against `record` (see the private helpers below).
    // Pure function of this item's own state plus `record` -- computed
    // fresh on every call, not cached (see Item.cpp).
    CharacterDerivedStats CalculateDerivedStats(const ItemRecord& record) const;

private:
    // This item's magic-option roll contribution against `record`'s
    // matching `*_scale` columns -- decodes option_bits into a per-gate
    // deviation from baseline tier 2 (the inverse of RollOptionBits,
    // handlers/ItemConfirmNpc.cpp), each times its gate's scale column, in
    // the same damage/magic/defense/attack_speed/accuracy/critical_rate/
    // evasion_rate/movement_speed/hp_percent/ap_percent order RollOptionBits
    // packs option_bits in.
    CharacterDerivedStats CalculateOptionContribution(const ItemRecord& record) const;

    // This item's refine ("+N") growth at its current refine_level, against
    // `record`'s refine_group/refine_*_scale columns -- see Item.cpp for the
    // per-(refine_group, level) curve. refine_level == 0 contributes
    // nothing.
    CharacterDerivedStats CalculateRefineContribution(const ItemRecord& record) const;
};
