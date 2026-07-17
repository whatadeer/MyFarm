#include "core/crop.h"

#include "core/growth_timer.h"

namespace core {

// Pacing is deliberately short (minutes) so the loop is provable in one
// sitting - tunable here without touching anything else. Better crops
// grow slower but pay more Farming XP.
const CropSpecies kCropSpeciesTable[kCropSpeciesCount] = {
    /* kCropWheat       */ {"Wheat", 4, 90, kItemWheatSeed, kItemWheat, 10},
    /* kCropTurnip      */ {"Turnip", 4, 180, kItemTurnipSeed, kItemTurnip, 8},
    /* kCropCarrot      */ {"Carrot", 4, 45, kItemCarrotSeed, kItemCarrot, 6},
    /* kCropTomato      */ {"Tomato", 4, 150, kItemTomatoSeed, kItemTomato, 14},
    /* kCropPumpkin     */ {"Pumpkin", 4, 300, kItemPumpkinSeed, kItemPumpkin, 24},
    /* kCropCauliflower */ {"Cauliflower", 4, 210, kItemCauliflowerSeed, kItemCauliflower, 16},
    /* kCropEggplant    */ {"Eggplant", 4, 240, kItemEggplantSeed, kItemEggplant, 18},
    /* kCropLettuce     */ {"Lettuce", 4, 120, kItemLettuceSeed, kItemLettuce, 11},
    /* kCropRadish      */ {"Radish", 4, 60, kItemRadishSeed, kItemRadish, 7},
    /* kCropBeetroot    */ {"Beetroot", 4, 180, kItemBeetrootSeed, kItemBeetroot, 15},
    /* kCropStarfruit   */ {"Starfruit", 4, 360, kItemStarfruitSeed, kItemStarfruit, 30},
    /* kCropCucumber    */ {"Cucumber", 4, 150, kItemCucumberSeed, kItemCucumber, 13},
    /* kCropCorn        */ {"Corn", 4, 240, kItemCornSeed, kItemCorn, 20},
    /* kCropSunflower   */ {"Sunflower", 4, 300, kItemSunflowerSeed, kItemSunflower, 26},
};

int cropStage(const CropSpecies& species, int64_t plantedAt, int64_t now) {
    return stageFromElapsed(plantedAt, now, species.secondsPerStage, species.numStages);
}

int cropStageOf(const Tile& tile, int64_t now) {
    if (!tile.hasCrop || tile.cropSpeciesId >= kCropSpeciesCount) return 0;
    const CropSpecies& species = kCropSpeciesTable[tile.cropSpeciesId];
    int64_t elapsed = elapsedSeconds(tile.timestamp, now);
    if (tile.watered) elapsed += elapsed / 2; // watered = 1.5x growth
    int64_t stage = elapsed / species.secondsPerStage;
    int64_t maxStage = species.numStages - 1;
    if (stage > maxStage) stage = maxStage;
    return static_cast<int>(stage);
}

bool canPlant(const Tile& tile) {
    return tile.tilled && !tile.hasCrop;
}

bool plantCrop(Tile& tile, uint8_t speciesId, int64_t now) {
    if (!canPlant(tile)) return false;
    if (speciesId >= kCropSpeciesCount) return false;

    tile.hasCrop = true;
    tile.watered = false;
    tile.cropSpeciesId = speciesId;
    tile.timestamp = now;
    return true;
}

bool canHarvest(const Tile& tile, int64_t now) {
    if (!tile.hasCrop || tile.cropSpeciesId >= kCropSpeciesCount) return false;
    return cropStageOf(tile, now) == kCropSpeciesTable[tile.cropSpeciesId].numStages - 1;
}

ItemId harvestCrop(Tile& tile, int64_t now) {
    if (!canHarvest(tile, now)) return kItemNone;

    ItemId item = kCropSpeciesTable[tile.cropSpeciesId].harvestItem;
    tile.hasCrop = false;
    tile.watered = false;
    return item;
}

} // namespace core
