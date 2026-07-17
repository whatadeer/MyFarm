#pragma once

#include <cstdint>

#include "core/item_db.h"

namespace core {

// 3 rows of 8 on the bottom screen. Milestone 2's item set is ~29 kinds;
// with 5 slots held by tools this leaves room for most of it - chests
// (placeable, own 24 slots each) absorb the overflow.
constexpr int kInventorySlots = 24;

struct ItemStack {
    ItemId item = kItemNone;
    uint16_t count = 0;
};

class Inventory {
public:
    Inventory();

    // Adds up to `count` of `item`, stacking into existing slots first,
    // then empty slots, up to the item's maxStack. Returns how many were
    // actually added - may be less than requested if every slot is full.
    int add(ItemId item, int count);

    // Removes up to `count` of `item` from wherever it's stacked. Returns
    // false (no change at all) if fewer than `count` are held in total.
    bool remove(ItemId item, int count);

    int countOf(ItemId item) const;

    const ItemStack& slot(int index) const { return slots_[index]; }

    // Direct slot write, used only when restoring a save - bypasses the
    // stacking logic in add() so the exact saved layout comes back instead
    // of whatever add() would have reshuffled it into.
    void setSlot(int index, ItemId item, uint16_t count);

    static constexpr int slotCount() { return kInventorySlots; }

private:
    ItemStack slots_[kInventorySlots];
};

// The player's tool bar: one fixed slot per tool kind (see kToolBarOrder),
// drawn above the general inventory grid so tools never compete with
// crops/wood/fish for a slot. Deliberately separate from Inventory - a
// chest's own Inventory still stores everything (tools included, for
// whoever stashed one long ago) in its plain slot grid, untouched by this.
constexpr int kToolSlots = 6;
constexpr ItemId kToolBarOrder[kToolSlots] = {
    kItemAxe, kItemPickaxe, kItemHoe, kItemWateringCan, kItemHammer, kItemFishingRod,
};

inline bool isToolItem(ItemId item) {
    return item != kItemNone && item < kItemCount && kItemTable[item].category == ItemCategory::Tool;
}

// Which kToolBarOrder slot `item` belongs in, or -1 if it isn't one of the
// fixed tool kinds. The Watering Can's empty/full forms share a slot.
inline int toolBarIndexFor(ItemId item) {
    ItemId key = item == kItemWateringCanFull ? kItemWateringCan : item;
    for (int i = 0; i < kToolSlots; i++) {
        if (kToolBarOrder[i] == key) return i;
    }
    return -1;
}

class ToolBelt {
public:
    ToolBelt();

    const ItemStack& slot(int index) const { return slots_[index]; }

    // Direct slot write, used only when restoring a save.
    void setSlot(int index, ItemId item, uint16_t count);

    // Places a tool (must satisfy isToolItem()) into its fixed slot,
    // overwriting whatever was there - e.g. the Watering Can swapping to
    // its Full form lands right back in the same slot. No-op if `item`
    // isn't a recognized tool kind.
    void add(ItemId item);

    // Clears the slot if it currently holds exactly `item`. Returns false
    // (no change) if it didn't.
    bool remove(ItemId item);

    // 1 if `item`'s slot is occupied (by `item` or its other phase, e.g.
    // Watering Can/Watering Can Full share a slot), else 0.
    int countOf(ItemId item) const;

    static constexpr int slotCount() { return kToolSlots; }

private:
    ItemStack slots_[kToolSlots];
};

} // namespace core
