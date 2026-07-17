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

// Gates span the full 1-100 ladder (the curve caps at 100 - skills.cpp).
// The levelReq/xp here are the MEADOW baselines for trees; the per-biome
// variant tables below override them for tiers 0-1.
constexpr NodeBalance kTreeBalance = {
    Skill::Logging, {1, 12, 65}, {10, 30, 120}, {1, 2, 3}, {480, 720, 960}};
constexpr NodeBalance kRockBalance = {
    Skill::Mining, {1, 25, 50}, {12, 40, 80}, {1, 2, 3}, {600, 900, 1200}};
constexpr NodeBalance kBushBalance = {
    Skill::Foraging, {1, 20, 45}, {8, 30, 60}, {1, 2, 3}, {300, 450, 600}};
// Mushrooms train (and gate on) Mycology, not Foraging - fungus is its
// own discipline, split out in save v13.
constexpr NodeBalance kMushroomBalance = {
    Skill::Mycology, {1, 15, 40}, {6, 26, 55}, {1, 2, 3}, {360, 540, 720}};

// Each tree VARIATION is its own unlock: the biome ladder (meadow ->
// birch -> cherry -> pine -> snow) climbs with distance from home, and
// the big trees climb it again. Fruit trees top the Logging ladder.
// Indexed by static_cast<int>(Biome); yield/respawn stay per-tier above.
constexpr int kTreeReqByBiome[5] = {1, 8, 16, 24, 32};      // slim (tier 0)
constexpr int kTreeXpByBiome[5] = {10, 18, 26, 36, 48};
constexpr int kTreeBigReqByBiome[5] = {12, 22, 32, 42, 52}; // big (tier 1)
constexpr int kTreeBigXpByBiome[5] = {30, 44, 58, 74, 92};
constexpr int kTreeFruitReq = 65; // fruit tree (tier 2), any biome
constexpr int kTreeFruitXp = 120;
inline int treeLevelReq(int tier, int biome) {
    if (tier >= 2) return kTreeFruitReq;
    return tier == 1 ? kTreeBigReqByBiome[biome] : kTreeReqByBiome[biome];
}
inline int treeXp(int tier, int biome) {
    if (tier >= 2) return kTreeFruitXp;
    return tier == 1 ? kTreeBigXpByBiome[biome] : kTreeXpByBiome[biome];
}
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

// Chickens tame from Herding 1; cows are for proven herders.
constexpr int kTameCowMinHerding = 12;

// Per-color tastes: each animal variant fancies exactly TWO foods. The
// ask bubble rolls ONE of them to show per refusal - the animal keeps
// the rest of its palate secret - and feeding either one tames it.
// Indexed by the wild animal's variant (0 = the classic free-pack look).
struct AnimalTaste {
    ItemId a;
    ItemId b;
};
constexpr AnimalTaste kChickenTastes[5] = {
    {kItemBerries, kItemWheat},
    {kItemWheat, kItemCarrot},
    {kItemBerries, kItemRadish},
    {kItemCarrot, kItemLettuce},
    {kItemRadish, kItemMushroom},
};
constexpr AnimalTaste kCowTastes[5] = {
    {kItemHay, kItemTurnip},
    {kItemTurnip, kItemApple},
    {kItemHay, kItemPumpkin},
    {kItemOrange, kItemCarrot},
    {kItemPear, kItemCorn},
};
// How many of the fancied food a tame consumes.
constexpr int kTameFoodChicken = 3;
constexpr int kTameFoodCow = 2;

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

// Fishing: rare sunken-treasure catch (needs real skill first).
constexpr int kTreasureChancePct = 5;
constexpr int kTreasureMinLevel = 30;
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

// --- Sprinting & stamina (save v12) -----------------------------------
// Circle Pad pushed to its rim = sprint; the D-Pad always walks. On land
// it trains Athletics (levels grow the pool), in open water it's a fast
// swim that trains Swimming (levels cut the swim drain).
constexpr float kRunStickThreshold = 0.88f; // fraction of full deflection
constexpr float kRunSpeedMul = 1.55f;
constexpr float kSwimSprintMul = 1.7f; // hard swim ~= 3 tiles/s, still slower than walking
constexpr float kStaminaBase = 100.0f;
constexpr float kStaminaPerAthleticsLevel = 8.0f;
constexpr float kRunStaminaPerSec = 10.0f;  // ~10s of sprint at level 1
constexpr float kSwimStaminaPerSec = 14.0f; // water is harder work
constexpr float kSwimDrainCutPerLevel = 0.03f; // per Swimming level above 1...
constexpr float kSwimDrainFloor = 0.4f;        // ...down to this fraction
constexpr float kStaminaRegenPerSec = 14.0f;
constexpr float kStaminaRegenSwimMul = 0.5f; // treading water recovers slower
constexpr float kExhaustRecoverFrac = 0.25f; // winded until refilled to this
constexpr float kSprintXpPerSec = 1.5f;      // Athletics or Swimming trickle

} // namespace core
