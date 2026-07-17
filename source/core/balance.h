#pragma once

#include <cstdint>

#include "core/item_db.h"
#include "core/skills.h"

namespace core {

// Every tunable number in one place. Real-time durations are deliberately
// short (minutes, not hours) so the loop is provable in a play session -
// stretch them here once the pacing feels right.

constexpr int kNodeTiers = 3;

// Per resource-node kind, indexed by tier (0..2). Tiers appear farther from
// spawn (see chunk.cpp) and are procedurally recolored variants of the same
// art - visibly "special" versions that need higher skill to harvest.
struct NodeBalance {
    Skill skill;
    int levelReq[kNodeTiers];
    int xp[kNodeTiers];
    int baseYield[kNodeTiers];
    int32_t respawnSec[kNodeTiers];
};

constexpr NodeBalance kTreeBalance = {
    Skill::Logging, {1, 5, 12}, {10, 22, 40}, {1, 2, 3}, {480, 720, 960}};
constexpr NodeBalance kRockBalance = {
    Skill::Mining, {1, 5, 12}, {12, 26, 45}, {1, 2, 3}, {600, 900, 1200}};
constexpr NodeBalance kBushBalance = {
    Skill::Foraging, {1, 4, 9}, {8, 18, 30}, {1, 2, 3}, {300, 450, 600}};
constexpr NodeBalance kMushroomBalance = {
    Skill::Foraging, {1, 3, 7}, {6, 14, 24}, {1, 2, 3}, {360, 540, 720}};
// Wild pumpkin/sunflower patches (single tier - clumps in the meadow).
constexpr NodeBalance kWildPatchBalance = {
    Skill::Foraging, {1, 1, 1}, {8, 8, 8}, {1, 1, 1}, {600, 600, 600}};
constexpr int kWildPatchSeedChancePct = 30; // foraging one -> its seed
// Wild plants live on a personal staggered clock (see wildStageAt):
// mostly ripe at any moment, with a scattering of sprouts and
// half-growns cycling through. One phase step per this many seconds.
constexpr int32_t kWildGrowSec = 240;

// Bonus-drop chances, in percent.
constexpr int kSaplingChancePct = 25;      // chopping a tree
constexpr int kOreChancePct[kNodeTiers] = {10, 30, 50}; // mining, by tier
constexpr int kBushSeedChancePct = 30;     // foraging a bush -> a crop seed
constexpr int kDigStoneChancePct = 20;     // digging a hole
constexpr int kTillHayChancePct = 30;      // tilling a Grass tile
constexpr int kAppleChancePct = 60;        // chopping a tier-2 fruit tree

// Which crop seeds a bush of each tier can drop - better bushes teach
// better crops.
constexpr ItemId kSeedPoolT0[] = {kItemWheatSeed, kItemTurnipSeed, kItemCarrotSeed,
                                  kItemRadishSeed};
constexpr ItemId kSeedPoolT1[] = {kItemTomatoSeed, kItemLettuceSeed, kItemCucumberSeed,
                                  kItemCauliflowerSeed, kItemCarrotSeed};
constexpr ItemId kSeedPoolT2[] = {kItemPumpkinSeed, kItemEggplantSeed, kItemBeetrootSeed,
                                  kItemStarfruitSeed, kItemCornSeed, kItemTomatoSeed,
                                  kItemSunflowerSeed};
constexpr int kSeedPoolSizes[kNodeTiers] = {4, 5, 7};

// Wild blooms: flowers pop up on pristine grass, a different scatter
// every 3-hour block (pure function of seed/tile/time - nothing stored).
// Foraging one gives berries, sometimes a seed; the spot stays picked
// until the next bloom cycle via the tile's timestamp.
constexpr int32_t kBloomBlockSec = 10800;
constexpr int kBloomOneIn = 150; // chance per pristine grass tile
constexpr int kXpBloom = 4;
constexpr int kBloomSeedChancePct = 30;

// Toolless fruit picking: standing at a ready fruit tree with anything
// but an Axe equipped lets you take one piece of fruit without felling
// it. Cooldown re-uses the tile's timestamp field (meaningless while the
// tree isn't depleted) so it can't be spammed.
constexpr int32_t kFruitPickCooldownSec = 90;
constexpr int kXpFruitPick = 4;

// Farming XP (gathering XP lives in the node tables above).
constexpr int kXpTill = 2;
constexpr int kXpPlant = 3;

// Herding.
constexpr int kXpTame = 30;
constexpr int kXpCollect = 6;
constexpr int32_t kEggIntervalSec = 300;
constexpr int32_t kMilkIntervalSec = 480;
constexpr int kCoopCapacity = 4;  // chickens per coop
constexpr int kBarnCapacity = 2;  // cows per barn
inline int tameCapacity(int herdingLevel) { return 1 + herdingLevel; } // total animals

// A freshly tamed animal is a baby; it grows up (and starts producing) on
// real time, like everything else.
constexpr int32_t kBabyGrowSec = 600;

// Taming food (consumed on a successful tame).
constexpr int kTameChickenBerries = 3;
constexpr int kTameCowHay = 2;
constexpr int kTameCowTurnips = 2; // fallback if no hay held
constexpr int kTameCowApples = 2;  // cows also love apples

// Beehive: produces Honey on its own clock, no animal needed. Collecting
// grants Foraging XP.
constexpr int32_t kHoneyIntervalSec = 420;
constexpr int kXpHoney = 5;

// Fishing: cast (A with the rod on water), wait a random bite delay, then
// hit A inside the catch window. Catch tier is weighted by Fishing level:
// weight_small = 100, weight_med = 12 + 6*level, weight_big = 4*level -
// so early levels are nearly all Minnows and a Lunker needs real levels.
constexpr float kBiteMinSec = 2.0f;
constexpr float kBiteMaxSec = 5.5f;
constexpr float kCatchWindowSec = 1.2f;
constexpr int kXpFish[3] = {8, 16, 30}; // small / med / big
inline void fishWeights(int fishingLevel, int* wSmall, int* wMed, int* wBig) {
    *wSmall = 100;
    *wMed = 12 + 6 * fishingLevel;
    *wBig = 4 * fishingLevel;
}

// Yield bonuses: +1 item per this many levels in the relevant skill.
constexpr int kYieldBonusLevels = 8;
inline int yieldBonus(int level) { return level / kYieldBonusLevels; }

// Sapling planted in a hole matures on the tier-0 tree respawn clock.
constexpr int32_t kSaplingGrowSec = 480;

// Bare dirt (un-tilled beds, filled holes, dug-up paths) heals back to
// grass after this long - the land recovers if you leave it alone.
constexpr int32_t kGrassRegrowSec = 1200;

// Build costs (wood, stone), spent directly from inventory when the
// Hammer places a building - see WorldScene::buildFromMaterials.
struct BuildCost {
    int wood;
    int stone;
};
constexpr BuildCost kCostWorkbench = {6, 0}; // wood only - buildable bare-handed
constexpr BuildCost kCostFence = {2, 0};
constexpr BuildCost kCostGate = {3, 0};
constexpr BuildCost kCostGateRight = {3, 0};
constexpr BuildCost kCostPath = {0, 1};
constexpr BuildCost kCostPathDirt = {1, 0};
constexpr BuildCost kCostPathPlank = {1, 0};
constexpr BuildCost kCostRail = {1, 1};
constexpr BuildCost kCostBridge = {3, 0};
constexpr BuildCost kCostCamp = {10, 0};
constexpr BuildCost kCostCoop = {15, 0};
constexpr BuildCost kCostBarn = {20, 8};
constexpr BuildCost kCostChest = {8, 0};
constexpr BuildCost kCostWell = {4, 10};
constexpr BuildCost kCostBeehive = {8, 0};
constexpr BuildCost kCostCampfire = {4, 2};
constexpr BuildCost kCostLamp = {4, 2};
constexpr BuildCost kCostChair = {3, 0};
constexpr BuildCost kCostRug = {2, 0};
constexpr BuildCost kCostRugLong = {3, 0};
constexpr BuildCost kCostBed = {8, 0};
constexpr BuildCost kCostTable = {5, 0};
constexpr BuildCost kCostDresser = {6, 0};
constexpr BuildCost kCostStool = {2, 0};
constexpr BuildCost kCostBench = {4, 0};
constexpr BuildCost kCostSign = {2, 0};
constexpr BuildCost kCostMailbox = {3, 1};
// Modular house building: cheap floors, walls in bulk, one nice door.
constexpr BuildCost kCostWall = {3, 0};
constexpr BuildCost kCostRoof = {4, 0};
constexpr BuildCost kCostFloor = {1, 0};
constexpr BuildCost kCostDoor = {4, 0};
constexpr BuildCost kCostXmasTree = {8, 4};
// Village homes - grand endgame builds - and a friend made of snow.
constexpr BuildCost kCostCottage = {30, 10};
constexpr BuildCost kCostHut = {20, 5};
constexpr BuildCost kCostManor = {40, 25};
constexpr BuildCost kCostSnowman = {0, 5};
// Homestead update: animal-area furnishings, waterside decor, presents,
// and the chest upgrade tiers.
constexpr BuildCost kCostTrough = {4, 0};
constexpr BuildCost kCostHayBale = {2, 0};
constexpr BuildCost kCostWaterTray = {2, 2};
constexpr BuildCost kCostBoat = {8, 0};
constexpr BuildCost kCostPicnic = {4, 0};
constexpr BuildCost kCostPresent = {2, 0};
constexpr BuildCost kCostChestSilver = {8, 6};
constexpr BuildCost kCostChestGold = {8, 14};

// Home-interior expansion: growing a Cottage/Hut/Manor room by one full
// column (A on an end wall inside) is a serious construction project.
constexpr BuildCost kCostExpand = {100, 50};

// The Clone Mirror: the endgame automaton. On top of the raw materials
// it takes kCloneGemCount of EACH deep-mine gem (ruby/diamond/emerald/
// amethyst) - the "specialty crystals" that power it. Only one clone
// can exist at a time.
constexpr BuildCost kCostClone = {150, 150};
constexpr int kCloneGemCount = 5;
// How the clone works: one action per kCloneWorkSec within
// kCloneWorkRadius tiles of where it stands; yields go into a chest
// within kCloneChestRadius (or drop at its feet).
constexpr float kCloneWorkSec = 2.5f;
constexpr int kCloneWorkRadius = 10;
constexpr int kCloneChestRadius = 8;

// Building XP per unit of material spent placing something - bigger
// builds teach more. Levels gate the buildable list (see kBuildables).
constexpr int kXpBuildPerMaterial = 2;

// Fishing: rare sunken-treasure catch (needs some skill first).
constexpr int kTreasureChancePct = 5;
constexpr int kTreasureMinLevel = 4;
constexpr int kXpTreasure = 25;

// Rain makes fish bite faster (multiplier on the bite delay).
constexpr float kRainBiteMul = 0.55f;

// --- The Mine (Milestone 3) -------------------------------------------------
constexpr BuildCost kCostMineShaft = {10, 5};
constexpr int kMaxHp = 6; // three hearts, two HP each
constexpr float kAttackCooldownSec = 0.4f;
constexpr float kInvulnSec = 1.0f;
constexpr int kXpKillSlime = 8;
constexpr int kXpKillBat = 6;

} // namespace core
