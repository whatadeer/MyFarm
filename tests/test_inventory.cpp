// Host-native tests for source/core/inventory.h: stacking, overflow, and
// removal bookkeeping.
#include "core/inventory.h"
#include "minitest.h"

using namespace core;

static void test_add_new_item_creates_slot() {
    Inventory inv;
    int added = inv.add(kItemWheatSeed, 5);
    CHECK(added == 5);
    CHECK(inv.countOf(kItemWheatSeed) == 5);
}

static void test_add_stacks_into_existing_slot() {
    Inventory inv;
    inv.add(kItemWheatSeed, 5);
    inv.add(kItemWheatSeed, 10);
    CHECK(inv.countOf(kItemWheatSeed) == 15);
}

static void test_add_respects_max_stack_and_overflow_spills_to_new_slot() {
    Inventory inv; // Wheat Seed maxStack is 99
    inv.add(kItemWheatSeed, 99);
    inv.add(kItemWheatSeed, 5); // first slot full, should spill to a new slot
    CHECK(inv.countOf(kItemWheatSeed) == 104);
    CHECK(inv.slot(0).count == 99);
    CHECK(inv.slot(1).item == kItemWheatSeed);
    CHECK(inv.slot(1).count == 5);
}

static void test_add_returns_partial_when_inventory_full() {
    Inventory inv;
    // kItemAxe's maxStack is 1, so each add() call claims exactly one slot -
    // fill all of them so nothing can stack and no empty slot remains.
    for (int i = 0; i < Inventory::slotCount(); i++) {
        inv.add(kItemAxe, 1);
    }
    int added = inv.add(kItemWheatSeed, 3);
    CHECK(added == 0);
}

static void test_remove_succeeds_and_clears_empty_slot() {
    Inventory inv;
    inv.add(kItemTurnipSeed, 3);
    CHECK(inv.remove(kItemTurnipSeed, 3));
    CHECK(inv.countOf(kItemTurnipSeed) == 0);
    CHECK(inv.slot(0).item == kItemNone);
}

static void test_remove_more_than_held_fails_cleanly() {
    Inventory inv;
    inv.add(kItemTurnipSeed, 2);
    CHECK(!inv.remove(kItemTurnipSeed, 5));
    CHECK(inv.countOf(kItemTurnipSeed) == 2); // unchanged
}

static void test_remove_partial_across_two_slots() {
    Inventory inv;
    inv.add(kItemWheatSeed, 99);
    inv.add(kItemWheatSeed, 20); // spills into a second slot
    CHECK(inv.remove(kItemWheatSeed, 100));
    CHECK(inv.countOf(kItemWheatSeed) == 19);
}

int main() {
    printf("test_inventory:\n");
    RUN(test_add_new_item_creates_slot);
    RUN(test_add_stacks_into_existing_slot);
    RUN(test_add_respects_max_stack_and_overflow_spills_to_new_slot);
    RUN(test_add_returns_partial_when_inventory_full);
    RUN(test_remove_succeeds_and_clears_empty_slot);
    RUN(test_remove_more_than_held_fails_cleanly);
    RUN(test_remove_partial_across_two_slots);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
