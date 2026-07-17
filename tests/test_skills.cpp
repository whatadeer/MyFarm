// Host-native tests for source/core/skills.h: the unbounded XP curve and
// level derivation.
#include "core/skills.h"
#include "minitest.h"

using namespace core;

static void test_curve_is_strictly_increasing() {
    for (int level = 1; level < 60; level++) {
        CHECK(xpToNext(level + 1) > xpToNext(level));
    }
}

static void test_level_boundaries() {
    CHECK(levelForXp(0) == 1);
    uint32_t toTwo = xpToNext(1);
    CHECK(levelForXp(toTwo - 1) == 1);
    CHECK(levelForXp(toTwo) == 2);
    CHECK(levelForXp(toTwo + xpToNext(2) - 1) == 2);
    CHECK(levelForXp(toTwo + xpToNext(2)) == 3);
}

static void test_huge_xp_terminates_and_is_high() {
    // u32 max XP must resolve without hanging, and lands exactly on the
    // level-100 cap (~99k lifetime XP caps the curve; u32 max is far past).
    int level = levelForXp(0xFFFFFFFFu);
    CHECK(level == 100);
}

static void test_progress_reporting() {
    int into = -1, span = -1;
    xpProgress(0, &into, &span);
    CHECK(into == 0);
    CHECK(span == static_cast<int>(xpToNext(1)));

    // Partway into level 2.
    uint32_t xp = xpToNext(1) + 5;
    xpProgress(xp, &into, &span);
    CHECK(into == 5);
    CHECK(span == static_cast<int>(xpToNext(2)));
}

static void test_early_levels_come_fast() {
    // A couple of tier-0 gathers should reach level 2 - the "hook them
    // early" end of the curve. 5 tree chops at 10 XP = 50 >= xpToNext(1).
    CHECK(xpToNext(1) <= 50);
}

int main() {
    printf("test_skills:\n");
    RUN(test_curve_is_strictly_increasing);
    RUN(test_level_boundaries);
    RUN(test_huge_xp_terminates_and_is_high);
    RUN(test_progress_reporting);
    RUN(test_early_levels_come_fast);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
