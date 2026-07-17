#include "core/inventory.h"

namespace core {

Inventory::Inventory() {
    for (auto& s : slots_) {
        s.item = kItemNone;
        s.count = 0;
    }
}

int Inventory::add(ItemId item, int count) {
    if (item == kItemNone || item >= kItemCount || count <= 0) return 0;

    const uint16_t maxStack = kItemTable[item].maxStack;
    int remaining = count;

    for (int i = 0; i < kInventorySlots && remaining > 0; i++) {
        if (slots_[i].item == item && slots_[i].count < maxStack) {
            int space = maxStack - slots_[i].count;
            int added = space < remaining ? space : remaining;
            slots_[i].count += static_cast<uint16_t>(added);
            remaining -= added;
        }
    }
    for (int i = 0; i < kInventorySlots && remaining > 0; i++) {
        if (slots_[i].item == kItemNone) {
            int added = maxStack < remaining ? maxStack : remaining;
            slots_[i].item = item;
            slots_[i].count = static_cast<uint16_t>(added);
            remaining -= added;
        }
    }
    return count - remaining;
}

int Inventory::countOf(ItemId item) const {
    int total = 0;
    for (const auto& s : slots_) {
        if (s.item == item) total += s.count;
    }
    return total;
}

bool Inventory::remove(ItemId item, int count) {
    if (count <= 0) return true;
    if (countOf(item) < count) return false;

    int remaining = count;
    for (int i = 0; i < kInventorySlots && remaining > 0; i++) {
        if (slots_[i].item == item) {
            int take = slots_[i].count < remaining ? slots_[i].count : remaining;
            slots_[i].count -= static_cast<uint16_t>(take);
            remaining -= take;
            if (slots_[i].count == 0) slots_[i].item = kItemNone;
        }
    }
    return true;
}

void Inventory::setSlot(int index, ItemId item, uint16_t count) {
    slots_[index].item = item;
    slots_[index].count = count;
}

ToolBelt::ToolBelt() {
    for (auto& s : slots_) {
        s.item = kItemNone;
        s.count = 0;
    }
}

void ToolBelt::setSlot(int index, ItemId item, uint16_t count) {
    slots_[index].item = item;
    slots_[index].count = count;
}

void ToolBelt::add(ItemId item) {
    int idx = toolBarIndexFor(item);
    if (idx < 0) return;
    slots_[idx].item = item;
    slots_[idx].count = 1;
}

bool ToolBelt::remove(ItemId item) {
    int idx = toolBarIndexFor(item);
    if (idx < 0 || slots_[idx].item != item) return false;
    slots_[idx].item = kItemNone;
    slots_[idx].count = 0;
    return true;
}

int ToolBelt::countOf(ItemId item) const {
    int idx = toolBarIndexFor(item);
    if (idx < 0) return 0;
    return slots_[idx].item != kItemNone ? 1 : 0;
}

} // namespace core
