#pragma once

#include <cstdint>

#include "core/item_db.h"
#include "core/tile.h"

namespace core {

struct CropSpecies {
    const char* name;
    uint8_t numStages;
    int32_t secondsPerStage;
    ItemId seedItem;
    ItemId harvestItem;
    int harvestXp; // Farming XP per harvest
};

// Wheat/Turnip art comes from the free pack's Basic Plants.png; the other
// three rows come from the premium pack's Farming Plants sheets. Seeds for
// the better crops drop from higher-tier bushes (see balance.h).
enum CropSpeciesId : uint8_t {
    kCropWheat = 0,
    kCropTurnip = 1,
    kCropCarrot = 2,
    kCropTomato = 3,
    kCropPumpkin = 4,
    kCropCauliflower = 5,
    kCropEggplant = 6,
    kCropLettuce = 7,
    kCropRadish = 8,
    kCropBeetroot = 9,
    kCropStarfruit = 10,
    kCropCucumber = 11,
    kCropCorn = 12,      // tall - its late growth stages draw 16x32
    kCropSunflower = 13, // tall too (Sorry pack "plants v2" sheet)
    kCropSpeciesCount = 14,
};

extern const CropSpecies kCropSpeciesTable[kCropSpeciesCount];

// Growth stage in [0, numStages-1] for a crop planted at `plantedAt`, as of
// `now` (both unix seconds). Clamped at both ends: a system clock that
// moved backwards never produces a negative elapsed time, and a very long
// absence never exceeds the final stage. This exact formula runs every
// frame during play *and* once on load/resume to fast-forward a crop that
// grew while the app was closed - no separate "catch-up" code path.
int cropStage(const CropSpecies& species, int64_t plantedAt, int64_t now);

// Stage for a specific tile - same math, but a WATERED crop counts its
// elapsed time at 1.5x (the watering can's whole purpose).
int cropStageOf(const Tile& tile, int64_t now);

bool canPlant(const Tile& tile);
bool plantCrop(Tile& tile, uint8_t speciesId, int64_t now);

bool canHarvest(const Tile& tile, int64_t now);

// Clears the crop (tile stays tilled, ready to replant) and returns the
// harvested item id, or kItemNone if the tile wasn't actually harvestable.
ItemId harvestCrop(Tile& tile, int64_t now);

} // namespace core
