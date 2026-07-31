#pragma once

#include <array>
#include <cstdint>

class PayloadWriter;

// One slot in GC_INVENTORY_ITEM_LIST's 256-slot array. Confirmed against
// real captures (OpenShiltz's game/handlers/gc_inventory_item_list.py) by
// cross-referencing populated slots' `item_id` against unsealed's decoded
// `item.edt`, AND confirmed live by sending specific ids/values to a real
// client and reading back what it rendered. Ids beyond item.edt's own
// ~13,318-entry range are REAL items, not garbage -- confirmed live (id
// 25806 rendered as a genuine "[Login] Churu Pet Food"); they just live in
// the higher `item.ed<n>` shards inside `item.edp` rather than the loose
// `item.edt` this project has a name table for.
//
// `refine` is genuinely ONE wire field with TWO live-confirmed display
// rules, split by the item's `item_type` in item.edt:
//   - equippable types (armor 16/26, weapon 6, shoes 20, crown 17,
//     pet/mount 22): client shows it AS-IS, as "+N" refine/enhancement
//     level. e.g. `refine=5` on id 200 (Warrior's Leather Clothes.G(T))
//     rendered as "+5" exactly; `refine=7` on id 109 (Extraordinary Piya,
//     a pet/mount) rendered as "+7" exactly.
//   - stackable/consumable types (potion 1, gem 23, and apparently the
//     unresolved high-id consumables too): client shows `refine + 1` as
//     the stack count. e.g. `refine=6` on id 112 (Red Potion) rendered as
//     "7 pcs"; `refine=8` on id 434 (Diamond) rendered as "9 pcs";
//     `refine=9` on id 25806 (Churu Pet Food) rendered as "10 pcs".
//     **To display a desired quantity N on a stackable item, set
//     `refine = N - 1`, not N.**
// The broad item_type=0 "general material" bucket is still unconfirmed
// either way (real captures show both 0 and real-looking counts) -- not
// covered by the live test above, which only touched types 1/16/22/23.
struct InventoryItemSlot
{
    std::uint32_t item_id = 0;
    std::uint32_t qty_or_refine = 0;
    std::array<std::uint8_t, 8> tail{};

    void Serialize(PayloadWriter& writer) const;
};

// GC_INVENTORY_ITEM_LIST (wire code 0x0007ce67 / 511591, s2c) -- inventory
// AND equipment contents in one array, sent as part of the post-CG_ENTER
// load burst. Body is CONFIRMED fixed at exactly 4104 bytes in every real
// capture: 4-byte `total_count` + 256 x 16-byte InventoryItemSlot records
// (4096 bytes) + 4 trailing zero/reserved bytes.
//
// `total_count` is NOT a naive item tally -- one real capture reads `0`
// here with 45 populated slots elsewhere in the same packet. It's more
// likely inventory capacity or a "gauge/pager" UI trigger (per the client
// trace in gc_inventory_item_list.py); left as a plain field since its real
// meaning isn't nailed down.
//
// Slot index == array position (0-based), CONFIRMED against real captures
// to match the client's own paperdoll + bag layout:
//   0 headgear, 1 top, 2 bottom, 3 shoes, 4 weapon, 5 (unconfirmed),
//   6 accessories, 7 pet, 8-12 unconfirmed (reserved/more accessory?),
//   13+ = general inventory bag, left-to-right/top-to-bottom.
//
// IMPORTANT: sending fewer than the full 4104 bytes (e.g. just
// `total_count`) leaves the client's parser reading past the end of our
// packet into whatever follows in its receive buffer -- observed in
// practice as equipment slots rendering stray values that traced back to
// fields in the packet sent immediately before this one. Always send the
// full `slots` array (zero-filled entries are fine -- they mean "empty
// slot", CONFIRMED: item id 0 is a real blank placeholder entry in
// item.edt).
struct InventoryItemList
{
    static constexpr std::size_t kTotalSlots = 256;
    static constexpr std::size_t kBodySize = 4104;

    std::uint32_t total_count = 0;
    std::array<InventoryItemSlot, kTotalSlots> slots{};

    void Serialize(PayloadWriter& writer) const;
};
