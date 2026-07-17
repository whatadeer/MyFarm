// Host-native tests for source/core/crop.h: growth-stage timing math and
// the plant/harvest tile lifecycle.
#include "core/crop.h"
#include "minitest.h"

using namespace core;

static void test_stage_zero_at_planting() {
    const CropSpecies& wheat = kCropSpeciesTable[kCropWheat];
    CHECK(cropStage(wheat, 1000, 1000) == 0);
}

static void test_stage_advances_at_boundaries() {
    const CropSpecies& wheat = kCropSpeciesTable[kCropWheat]; // 4 stages, 90s/stage
    int64_t planted = 1000;
    CHECK(cropStage(wheat, planted, planted + 0) == 0);
    CHECK(cropStage(wheat, planted, planted + 89) == 0);
    CHECK(cropStage(wheat, planted, planted + 90) == 1);
    CHECK(cropStage(wheat, planted, planted + 179) == 1);
    CHECK(cropStage(wheat, planted, planted + 180) == 2);
    CHECK(cropStage(wheat, planted, planted + 270) == 3);
}

static void test_stage_clamps_at_final_stage_after_long_absence() {
    const CropSpecies& wheat = kCropSpeciesTable[kCropWheat];
    int64_t planted = 1000;
    int64_t threeDaysLater = planted + 3 * 24 * 3600; // way past the 360s it takes to mature
    CHECK(cropStage(wheat, planted, threeDaysLater) == wheat.numStages - 1);
}

static void test_clock_rollback_clamped_to_zero() {
    const CropSpecies& wheat = kCropSpeciesTable[kCropWheat];
    int64_t planted = 1000;
    CHECK(cropStage(wheat, planted, planted - 500) == 0); // now is *before* plantedAt
}

static void test_till_plant_harvest_lifecycle() {
    Tile tile;
    CHECK(canTill(tile));
    CHECK(tillTile(tile));

    CHECK(canPlant(tile));
    CHECK(plantCrop(tile, kCropWheat, 1000));
    CHECK(tile.hasCrop);
    CHECK(!canHarvest(tile, 1000)); // stage 0, not mature yet
    CHECK(!canHarvest(tile, 1000 + 269)); // one tick before maturity
    CHECK(canHarvest(tile, 1000 + 270)); // exactly at final stage

    ItemId got = harvestCrop(tile, 1000 + 270);
    CHECK(got == kItemWheat);
    CHECK(!tile.hasCrop);
    CHECK(tile.tilled); // stays tilled, ready to replant
}

static void test_cannot_plant_on_untilled_or_occupied_tile() {
    Tile tile;
    CHECK(!canPlant(tile)); // not tilled yet
    CHECK(!plantCrop(tile, kCropWheat, 1000));

    tillTile(tile);
    plantCrop(tile, kCropWheat, 1000);
    CHECK(!canPlant(tile)); // already occupied
    CHECK(!plantCrop(tile, kCropTurnip, 1000));
}

static void test_harvest_of_non_crop_tile_returns_none() {
    Tile tile;
    CHECK(harvestCrop(tile, 1000) == kItemNone);
}

static void test_watered_crops_grow_faster() {
    Tile tile;
    tillTile(tile);
    plantCrop(tile, kCropWheat, 1000); // 4 stages, 90s each -> mature at +270
    CHECK(!tile.watered);              // planting starts dry

    tile.watered = true;
    // Watered counts elapsed at 1.5x: mature at 270/1.5 = +180.
    CHECK(cropStageOf(tile, 1000 + 179) == 2);
    CHECK(cropStageOf(tile, 1000 + 180) == 3);
    CHECK(canHarvest(tile, 1000 + 180));
    CHECK(!canHarvest(tile, 1000 + 179));

    harvestCrop(tile, 1000 + 180);
    CHECK(!tile.watered); // harvest clears the watering
}

int main() {
    printf("test_crop:\n");
    RUN(test_stage_zero_at_planting);
    RUN(test_stage_advances_at_boundaries);
    RUN(test_stage_clamps_at_final_stage_after_long_absence);
    RUN(test_clock_rollback_clamped_to_zero);
    RUN(test_till_plant_harvest_lifecycle);
    RUN(test_cannot_plant_on_untilled_or_occupied_tile);
    RUN(test_harvest_of_non_crop_tile_returns_none);
    RUN(test_watered_crops_grow_faster);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
