#include "scenes/world_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "atlas.h"
#include "core/balance.h"
#include "core/clock.h"
#include "core/crop.h"
#include "core/daylight.h"
#include "core/growth_timer.h"
#include "core/wall_autotile.h"
#include "platform/audio.h"
#include "platform/log.h"
#include "platform/save_io.h"
#include "platform/screenshot.h"

namespace scenes {

namespace {

constexpr float kScreenTilePx = 32.0f; // 16px source sprite * 2x on-screen scale
constexpr float kSpriteScale = 2.0f;
constexpr float kTopScreenW = 400.0f;
constexpr float kTopScreenH = 240.0f;
constexpr float kMoveTilesPerSecond = 4.0f;
constexpr float kPathSpeedMul = 1.4f; // stone paths are worth building

// Wild-animal request bubble: total frames on screen, the leading slice
// spent on the pop-in droplet, and how long the cow lingers on each item
// of its menu (hay/turnip/fruit) before cycling to the next.
constexpr int kReqBubbleFrames = 140;
constexpr int kReqBubblePopFrames = 8;
constexpr int kReqBubbleCycleFrames = 35;

// Bottom screen (320x240) is two full-screen tabs (Inventory/Skills)
// cycled with L/R, plus a small header row naming the current one.
constexpr float kBottomW = 320.0f;
constexpr float kBottomH = 240.0f;
// Tall enough to fit the tab name, the clock, AND a real (not
// postage-stamp) weather icon in the top-right corner. The held item's
// name gets its own full-width line right below the bar - Skills tab
// only; Inventory shows the same info folded into its tool bar row below
// instead (see kToolBarH), which is where that vertical space goes.
constexpr float kTabHeaderH = 32.0f;
constexpr float kHeldLineY = kTabHeaderH + 2.0f;
constexpr float kTabContentY = kTabHeaderH + 24.0f; // Skills tab content start (inside its frame)

// Tan dialog-box frame rings around each tab's content (hollow 9-slices
// pre-composed by prep_assets at exact size, drawn at 2x). Inventory's
// spans everything below the header; Skills' starts below the held-item
// line. Borders are 8px on-screen (10px bottom lip) - the tab layouts
// below are inset to clear them.
constexpr float kFrameInvY = kTabHeaderH;
constexpr float kFrameSkillsY = 46.0f;
constexpr float kFrameBorder = 8.0f;

// Inventory tab's tool bar: one fixed slot per tool kind (see
// core::kToolBarOrder), using the same UI-kit slot frames as the general
// grid below, just smaller - sits right under the header, replacing the
// header's held-item line for this tab (the selected slot's white frame
// already shows what's equipped; a held-item name still prints beside the
// slots for non-tool selections and the Hammer's build-ghost info).
constexpr float kToolBarY = kFrameInvY + kFrameBorder;
constexpr float kToolBarH = 28.0f;
constexpr float kToolSlotPx = 26.0f;
constexpr float kToolBarGap = 2.0f;
constexpr float kToolBarX = 12.0f;
constexpr float kToolBarLabelX = kToolBarX + core::kToolSlots * (kToolSlotPx + kToolBarGap) + 6.0f;

// Inventory tab: 3x8 grid, 36px slots (8*36 = 288, centered inside the
// frame ring), starting right below the tool bar.
constexpr float kSlotPx = 36.0f;
constexpr int kInvCols = 8;
constexpr int kInvRows = 3;
constexpr float kInvGridX = 16.0f;
constexpr float kInvGridY = kToolBarY + kToolBarH;
constexpr float kStatusY = kInvGridY + kInvRows * kSlotPx + 2.0f;
// Room for 32px (2x-scaled) button glyphs above the frame's bottom lip.
constexpr float kHintY = 198.0f;

// Drop/Trash buttons act on the currently equipped slot (grid or tool
// bar) - sit between the status line and the bottom hint row.
constexpr float kDropBtnY = kStatusY + 8.0f;
constexpr float kDropBtnH = 18.0f;
constexpr float kDropBtnX = 168.0f;
constexpr float kDropBtnW = 68.0f;
constexpr float kTrashBtnX = 244.0f;
constexpr float kTrashBtnW = 68.0f;

// Skills tab: one row per skill, each with a full-width XP bar (25px
// keeps all seven inside the frame ring below the held-item line).
constexpr float kSkillRowH = 25.0f;
constexpr float kSkillBarX = 96.0f;
constexpr float kSkillBarW = 210.0f;

// Chest-transfer layout (36px slots so two 3x8 grids fit).
constexpr float kChestSlotPx = 36.0f;
constexpr float kChestGridX = 16.0f;
constexpr float kChestTopY = 16.0f;
constexpr float kChestBotY = 128.0f;

constexpr int kPauseOptionCount = 3;
const char* kPauseOptions[kPauseOptionCount] = {"Save", "Teleport Home", "Resume"};

// Buildables, ordered by the Building level that unlocks them (the X/Y
// cycle only shows what your level can build, so early on the list is
// short and grows as you level - natural progression).
struct Buildable {
    core::ItemId item;
    core::BuildCost cost;
    int level; // required Building skill level
};
constexpr Buildable kBuildables[] = {
    // Level 1: the crafting station, a starter camp, fences, comforts.
    {core::kItemWorkbench, core::kCostWorkbench, 1},
    {core::kItemCamp, core::kCostCamp, 1},
    {core::kItemFence, core::kCostFence, 1},
    {core::kItemGate, core::kCostGate, 1},
    {core::kItemGateRight, core::kCostGateRight, 1},
    {core::kItemPathDirt, core::kCostPathDirt, 1},
    {core::kItemRug, core::kCostRug, 1},
    {core::kItemSign, core::kCostSign, 1},
    {core::kItemCampfire, core::kCostCampfire, 1},
    // Level 2: sturdier paths and storage.
    {core::kItemPath, core::kCostPath, 2},
    {core::kItemPathPlank, core::kCostPathPlank, 2},
    {core::kItemChest, core::kCostChest, 2},
    {core::kItemChair, core::kCostChair, 2},
    {core::kItemStool, core::kCostStool, 2},
    {core::kItemSnowman, core::kCostSnowman, 2},
    // Level 3: real house parts (and a picnic for the meadow).
    {core::kItemFloor, core::kCostFloor, 3},
    {core::kItemWall, core::kCostWall, 3},
    {core::kItemDoor, core::kCostDoor, 3},
    {core::kItemBed, core::kCostBed, 3},
    {core::kItemWoodTable, core::kCostTable, 3},
    {core::kItemRugLong, core::kCostRugLong, 3},
    {core::kItemMailbox, core::kCostMailbox, 3},
    {core::kItemPicnic, core::kCostPicnic, 3},
    // Level 4: roofs, bridges, and finer furniture.
    {core::kItemRoof, core::kCostRoof, 4},
    {core::kItemBridge, core::kCostBridge, 4},
    {core::kItemBoat, core::kCostBoat, 4},
    {core::kItemLamp, core::kCostLamp, 4},
    {core::kItemDresser, core::kCostDresser, 4},
    {core::kItemBench, core::kCostBench, 4},
    {core::kItemBeehive, core::kCostBeehive, 4},
    {core::kItemChestSilver, core::kCostChestSilver, 4},
    // Level 5: infrastructure and husbandry.
    {core::kItemWell, core::kCostWell, 5},
    {core::kItemRail, core::kCostRail, 5},
    {core::kItemCoop, core::kCostCoop, 5},
    {core::kItemTrough, core::kCostTrough, 5},
    {core::kItemHayBale, core::kCostHayBale, 5},
    {core::kItemWaterTray, core::kCostWaterTray, 5},
    {core::kItemXmasTree, core::kCostXmasTree, 5},
    {core::kItemPresent, core::kCostPresent, 5},
    // Level 6+: the big projects.
    {core::kItemBarn, core::kCostBarn, 6},
    {core::kItemChestGold, core::kCostChestGold, 6},
    {core::kItemMineShaft, core::kCostMineShaft, 6},
    {core::kItemHut, core::kCostHut, 7},
    {core::kItemCottage, core::kCostCottage, 8},
    {core::kItemManor, core::kCostManor, 9},
    // Level 10: the Clone Mirror (also takes 5 of each deep-mine gem -
    // see buildFromMaterials' crystal check).
    {core::kItemClone, core::kCostClone, 10},
};
constexpr int kBuildableCount = static_cast<int>(sizeof(kBuildables) / sizeof(kBuildables[0]));

// Workbench recipes: every tool except the starter Axe is crafted here.
// Stone comes toolless from digging holes (and hammer-bonking rocks), so
// the stone-costing tools are reachable before owning a Pickaxe.
struct Recipe {
    core::ItemId item;
    core::BuildCost cost;
};
constexpr Recipe kRecipes[] = {
    {core::kItemHammer, {4, 0}},      {core::kItemHoe, {3, 0}},
    {core::kItemPickaxe, {3, 2}},     {core::kItemWateringCan, {2, 3}},
    {core::kItemFishingRod, {5, 0}},
};
constexpr int kRecipeCount = static_cast<int>(sizeof(kRecipes) / sizeof(kRecipes[0]));

// Visual-only hash for ground variety (grass variants, lily pads) -
// deterministic from coords so the world doesn't shimmer, but never stored.
uint32_t visHash(int32_t x, int32_t y) {
    uint32_t h = static_cast<uint32_t>(x) * 0x85EBCA6Bu ^ static_cast<uint32_t>(y) * 0xC2B2AE35u;
    h ^= h >> 13;
    h *= 0x27D4EB2Fu;
    h ^= h >> 16;
    return h;
}

int clampTier(uint8_t tier) {
    return tier >= core::kNodeTiers ? core::kNodeTiers - 1 : tier;
}

// Wild blooms: a different scatter of flowers appears on pristine grass
// every 3-hour block - a pure function of (tile, time), so nothing is
// stored until one is picked (picking stamps the tile's timestamp, hiding
// that spot until the next cycle). Returns the flower sprite, or -1.
int bloomSpriteAt(const core::Tile& tile, int32_t tx, int32_t ty, int64_t now) {
    if (tile.terrain != core::Terrain::Grass || tile.tilled || tile.hasCrop ||
        tile.decoration != core::Decoration::None || tile.placed != core::Placed::None) {
        return -1;
    }
    uint32_t block = static_cast<uint32_t>(now / core::kBloomBlockSec);
    uint32_t h = visHash(tx + static_cast<int32_t>(block) * 131,
                         ty - static_cast<int32_t>(block) * 57);
    if (h % core::kBloomOneIn != 0) return -1;
    if (tile.timestamp >= static_cast<int64_t>(block) * core::kBloomBlockSec) return -1;
    static const int kFlowers[6] = {atlas_flower_0_idx, atlas_flower_1_idx, atlas_flower_2_idx,
                                    atlas_flower_3_idx, atlas_flower_4_idx, atlas_flower_5_idx};
    return kFlowers[(h / 7) % 6];
}

} // namespace

void WorldScene::onEnter() {
    selectedSlot_ = -1;
    selectedTool_ = 0; // defaults to the tool bar's first slot (the Axe)
    paused_ = false;
    pauseSelection_ = 0;
    chestOpen_ = false;
    hudTab_ = HudTab::Inventory;
    buildGhostIdx_ = 0;
    statusFrames_ = 0;
    actionTimer_ = 0;
    wild_.clear();
    spawnTimer_ = 2.0f;
    rngState_ = static_cast<uint32_t>(core::nowSeconds()) ^ 0x9E3779B9u;
    if (rngState_ == 0) rngState_ = 1;
    platform::setMusicMood(core::darkness(core::nowSeconds()) > 0.5f ? core::Mood::Spooky
                                                                      : core::Mood::Calm);
    LOG("world onEnter: player=(%.1f,%.1f)", state_ ? state_->playerPos.x : -1.0f,
        state_ ? state_->playerPos.y : -1.0f);
}

uint32_t WorldScene::nextRand() {
    // xorshift32 - gameplay drops only; worldgen stays on its own
    // deterministic hash.
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    return rngState_;
}

bool WorldScene::rollPct(int pct) {
    return static_cast<int>(nextRand() % 100) < pct;
}

void WorldScene::setStatus(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(statusMsg_, sizeof(statusMsg_), fmt, args);
    va_end(args);
    statusFrames_ = 150;
}

void WorldScene::awardXp(core::Skill skill, int amount) {
    uint32_t& xp = state_->skillXp[static_cast<int>(skill)];
    bool firstEver = xp == 0;
    int before = core::levelForXp(xp);
    xp += static_cast<uint32_t>(amount);
    int after = core::levelForXp(xp);
    if (firstEver) {
        // Skills stay hidden ("???" in the Skills tab) until first used.
        setStatus("New skill discovered: %s!", core::kSkillNames[static_cast<int>(skill)]);
        platform::playSfx(platform::Sfx::LevelUp);
        emoteSprite_ = atlas_emote_cheer_idx;
        emoteExtra_ = -1;
        emoteT_ = 110;
        return;
    }
    if (after > before) {
        setStatus("%s Lv %d!", core::kSkillNames[static_cast<int>(skill)], after);
        if (skill == core::Skill::Building) {
            for (const Buildable& b : kBuildables) {
                if (b.level == after) {
                    setStatus("Building Lv %d - new blueprints!", after);
                    break;
                }
            }
        }
        platform::playSfx(platform::Sfx::LevelUp);
        emoteSprite_ = atlas_emote_cheer_idx;
        // The skill's signature tool/item pops up next to the cheer so you
        // can tell WHAT leveled without reading the status line.
        static const core::ItemId kSkillIcon[core::kSkillCount] = {
            core::kItemHoe,     core::kItemAxe, core::kItemPickaxe,  core::kItemBerries,
            core::kItemEgg,     core::kItemFishingRod, core::kItemHammer};
        emoteExtra_ = spriteForItem(kSkillIcon[static_cast<int>(skill)]);
        emoteT_ = 110;
    }
}

void WorldScene::selectItem(core::ItemId item) {
    if (core::isToolItem(item)) {
        int idx = core::toolBarIndexFor(item);
        if (idx < 0) return;
        selectedTool_ = idx;
        selectedSlot_ = -1;
        return;
    }
    for (int i = 0; i < core::Inventory::slotCount(); i++) {
        if (state_->inventory.slot(i).item == item) {
            selectedSlot_ = i;
            selectedTool_ = -1;
            return;
        }
    }
}

const core::ItemStack& WorldScene::selectedStack() const {
    static const core::ItemStack kEmpty{};
    if (selectedTool_ >= 0) return state_->toolBelt.slot(selectedTool_);
    if (selectedSlot_ < 0) return kEmpty;
    return state_->inventory.slot(selectedSlot_);
}

void WorldScene::dropSelected() {
    const core::ItemStack& s = selectedStack();
    if (s.item == core::kItemNone) {
        setStatus("Nothing equipped to drop.");
        return;
    }
    core::Vec2f off = facingOffset();
    int32_t tx = static_cast<int32_t>(std::floor(state_->playerPos.x + off.x));
    int32_t ty = static_cast<int32_t>(std::floor(state_->playerPos.y + off.y));
    core::GroundItem* spot = state_->groundItemAt(tx, ty);
    if (spot && spot->item != s.item) {
        // Faced tile already holds something else - try the tile you're
        // standing on instead before giving up.
        tx = static_cast<int32_t>(std::floor(state_->playerPos.x));
        ty = static_cast<int32_t>(std::floor(state_->playerPos.y));
        spot = state_->groundItemAt(tx, ty);
        if (spot && spot->item != s.item) {
            setStatus("No room to drop that here.");
            return;
        }
    }
    if (selectedTool_ >= 0) {
        if (!state_->toolBelt.remove(s.item)) return;
    } else if (!state_->inventory.remove(s.item, 1)) {
        return;
    }
    if (spot) {
        spot->count++;
    } else {
        core::GroundItem g;
        g.x = tx;
        g.y = ty;
        g.item = s.item;
        g.count = 1;
        state_->groundItems.push_back(g);
    }
    setStatus("Dropped %s", core::kItemTable[s.item].name);
    platform::playSfx(platform::Sfx::Place);
}

void WorldScene::trashSelected() {
    const core::ItemStack& s = selectedStack();
    if (s.item == core::kItemNone) {
        setStatus("Nothing equipped to trash.");
        return;
    }
    if (selectedTool_ >= 0) {
        state_->toolBelt.remove(s.item);
    } else {
        state_->inventory.remove(s.item, 1);
    }
    setStatus("Trashed %s", core::kItemTable[s.item].name);
    platform::playSfx(platform::Sfx::Demolish);
}

bool WorldScene::buildModeActive() const {
    return selectedStack().item == core::kItemHammer;
}

void WorldScene::cycleHudTab(int dir) {
    int t = (static_cast<int>(hudTab_) + dir + 2) % 2;
    hudTab_ = static_cast<HudTab>(t);
}

void WorldScene::cycleBuildGhost(int dir) {
    // Skip anything above the player's Building level - locked buildables
    // simply don't appear in the cycle until the level is earned.
    int lvl = state_ ? state_->skillLevel(core::Skill::Building) : 1;
    for (int i = 0; i < kBuildableCount; i++) {
        buildGhostIdx_ = (buildGhostIdx_ + dir + kBuildableCount) % kBuildableCount;
        if (kBuildables[buildGhostIdx_].level <= lvl) return;
    }
    buildGhostIdx_ = 0; // list starts with level-1 entries, always legal
}

void WorldScene::update(float dt, const platform::InputState& input) {
    if (!state_) return;

    animFrame_++;

    // Ambient music mood follows day/night - checked a couple of times a
    // second rather than every frame; setMusicMood() itself only takes
    // effect at the next bar boundary, so this doesn't need to be precise.
    if (animFrame_ % 30 == 0) {
        platform::setMusicMood(core::darkness(core::nowSeconds()) > 0.5f ? core::Mood::Spooky
                                                                          : core::Mood::Calm);
    }
    if (statusFrames_ > 0) statusFrames_--;
    if (actionTimer_ > 0) actionTimer_--;
    if (emoteT_ > 0) emoteT_--;
    // Age the tree-chop leaf-poofs; drop finished ones.
    for (size_t i = 0; i < poofs_.size();) {
        poofs_[i].t += dt;
        if (poofs_[i].t > 0.80f) {
            poofs_.erase(poofs_.begin() + static_cast<long>(i));
        } else {
            i++;
        }
    }

    // Grass regrowth sweep (every ~half second): bare dirt near the
    // player heals back to grass once its clock (stamped when the dirt
    // was bared) runs out. Old saves' unstamped dirt just stays dirt.
    if (animFrame_ % 32 == 0 && mineFloor_ == 0 && !inInterior()) {
        int64_t now = core::nowSeconds();
        int32_t pcx = static_cast<int32_t>(std::floor(state_->playerPos.x));
        int32_t pcy = static_cast<int32_t>(std::floor(state_->playerPos.y));
        for (int32_t ty = pcy - 8; ty <= pcy + 8; ty++) {
            for (int32_t tx = pcx - 10; tx <= pcx + 10; tx++) {
                core::Tile& t = state_->world.tileAt(tx, ty);
                if (t.terrain == core::Terrain::Dirt && !t.tilled && !t.hasCrop &&
                    t.decoration == core::Decoration::None && t.placed == core::Placed::None &&
                    t.timestamp != 0 &&
                    core::elapsedAtLeast(t.timestamp, now, core::kGrassRegrowSec)) {
                    t.terrain = core::Terrain::Grass;
                    t.timestamp = 0;
                    state_->world.markDirty(tx, ty);
                }
            }
        }
        // Occasional-spawn sweep: since mined rocks are gone for good,
        // the land makes new ones - a pebble fairly often, a fresh rock
        // now and then, on open ground just outside the camera.
        uint32_t spawnRoll = nextRand() % 128;
        if (spawnRoll < 10) {
            bool rock = spawnRoll == 0; // 1/128 rock, 9/128 pebble per sweep
            float ang = static_cast<float>(nextRand() % 628) / 100.0f;
            float dist = 8.0f + static_cast<float>(nextRand() % 60) / 10.0f;
            int32_t sxp = static_cast<int32_t>(std::floor(state_->playerPos.x + std::cos(ang) * dist));
            int32_t syp = static_cast<int32_t>(std::floor(state_->playerPos.y + std::sin(ang) * dist));
            core::Tile& t = state_->world.tileAt(sxp, syp);
            if ((t.terrain == core::Terrain::Grass || t.terrain == core::Terrain::Dirt) &&
                !t.tilled && !t.hasCrop && t.decoration == core::Decoration::None &&
                t.placed == core::Placed::None &&
                core::mazeAt(state_->worldSeed, sxp, syp) == 0) {
                t.decoration = rock ? core::Decoration::Rock : core::Decoration::Pebble;
                t.decoTier = 0;
                t.depleted = false;
                state_->world.markDirty(sxp, syp);
            }
        }
    }

    // SELECT: screenshot, anywhere - overworld, mine, even paused. The
    // framebuffer still holds the last presented frame at this point.
    if (input.selectPressed) {
        bool stereo = false;
        bool ok = platform::saveScreenshot(&stereo);
        setStatus(ok ? (stereo ? "3D screenshot saved!" : "Screenshot saved!")
                     : "Screenshot failed");
        platform::playSfx(ok ? platform::Sfx::Ui : platform::Sfx::Deny);
    }

    if (paused_) {
        handlePauseInput(input);
        return;
    }
    if (chestOpen_) {
        handleChestInput(input);
        return;
    }
    if (craftOpen_) {
        handleCraftInput(input);
        return;
    }

    if (input.menuPressed) {
        paused_ = true;
        pauseSelection_ = 0;
        fishState_ = 0;
        return;
    }

    if (mineFloor_ > 0) {
        updateMine(dt, input);
        return;
    }

    if (fishState_ != 0) {
        updateFishing(dt, input);
        updateWildAnimals(dt);
        updateClone(dt);
        return;
    }

    handleFieldInput(dt, input);
    updateWildAnimals(dt);
    updateClone(dt);
}

void WorldScene::updateFishing(float dt, const platform::InputState& input) {
    // (The dedicated rod-out waiting pose draws whenever fishState_ != 0 -
    // see playerSpriteIndex - so no actionTimer_ hold is needed here.)

    if (input.move != platform::MoveDir::None || input.cancelPressed) {
        fishState_ = 0;
        actionTimer_ = 0;
        setStatus("Reeled in.");
        return;
    }

    fishTimer_ -= dt;
    if (input.actionPressed) {
        if (fishState_ == 2) {
            resolveCatch();
        } else {
            fishState_ = 0;
            actionTimer_ = 0;
            setStatus("Too soon - it fled!");
            platform::playSfx(platform::Sfx::Deny);
        }
        return;
    }
    if (fishState_ == 1 && fishTimer_ <= 0.0f) {
        fishState_ = 2;
        fishTimer_ = core::kCatchWindowSec;
        setStatus("!!! Press A !!!");
        platform::playSfx(platform::Sfx::Bite);
    } else if (fishState_ == 2 && fishTimer_ <= 0.0f) {
        fishState_ = 0;
        actionTimer_ = 0;
        setStatus("It got away...");
        platform::playSfx(platform::Sfx::Deny);
    }
}

void WorldScene::resolveCatch() {
    fishState_ = 0;
    actionTimer_ = 20;

    int level = state_->skillLevel(core::Skill::Fishing);

    // Rare sunken treasure: a gem straight off the seabed.
    if (level >= core::kTreasureMinLevel &&
        static_cast<int>(nextRand() % 100) < core::kTreasureChancePct) {
        static const core::ItemId kGems[4] = {core::kItemRuby, core::kItemDiamond,
                                              core::kItemEmerald, core::kItemAmethyst};
        core::ItemId gem = kGems[nextRand() % 4];
        state_->inventory.add(gem, 1);
        awardXp(core::Skill::Fishing, core::kXpTreasure);
        setStatus("Sunken treasure! +1 %s", core::kItemTable[gem].name);
        platform::playSfx(platform::Sfx::LevelUp);
        return;
    }

    // Each tier is a whole pool of sea life (the full Fish Sprites sheet).
    static const core::ItemId kSmallCatch[4] = {core::kItemFishSmall, core::kItemShrimp,
                                                core::kItemClownfish, core::kItemSnail};
    static const core::ItemId kMedCatch[4] = {core::kItemFishMed, core::kItemCrab,
                                              core::kItemSeahorse, core::kItemOctopus};
    static const core::ItemId kBigCatch[4] = {core::kItemFishBig, core::kItemLobster,
                                              core::kItemRay, core::kItemTurtle};

    int wSmall, wMed, wBig;
    core::fishWeights(level, &wSmall, &wMed, &wBig);
    int roll = static_cast<int>(nextRand() % static_cast<uint32_t>(wSmall + wMed + wBig));
    int tier;
    core::ItemId fish;
    if (roll < wSmall) {
        tier = 0;
        fish = kSmallCatch[nextRand() % 4];
    } else if (roll < wSmall + wMed) {
        tier = 1;
        fish = kMedCatch[nextRand() % 4];
    } else {
        tier = 2;
        fish = kBigCatch[nextRand() % 4];
    }
    state_->inventory.add(fish, 1);
    awardXp(core::Skill::Fishing, core::kXpFish[tier]);
    setStatus("+1 %s!", core::kItemTable[fish].name);
    platform::playSfx(platform::Sfx::Splash);
}

void WorldScene::handleFieldInput(float dt, const platform::InputState& input) {
    // Riding a minecart: the track drives, the player just hangs on.
    // A or B hops out; at each tile center the cart continues straight if
    // it can, follows a corner if it must, and stops at the end of the
    // line. Placed rails snap movement to tile centers so turns are crisp.
    if (riding_) {
        if (input.actionPressed || input.cancelPressed) {
            riding_ = false;
            setStatus("Hopped out");
            return;
        }
        constexpr float kCartTilesPerSecond = 7.0f;
        core::Vec2f& p = state_->playerPos;
        p.x += static_cast<float>(rideDirX_) * kCartTilesPerSecond * dt;
        p.y += static_cast<float>(rideDirY_) * kCartTilesPerSecond * dt;
        int32_t cx = static_cast<int32_t>(std::floor(p.x));
        int32_t cy = static_cast<int32_t>(std::floor(p.y));
        if (cx != rideDecidedX_ || cy != rideDecidedY_) {
            float lx = p.x - static_cast<float>(cx), ly = p.y - static_cast<float>(cy);
            bool crossed = (rideDirX_ > 0 && lx >= 0.5f) || (rideDirX_ < 0 && lx <= 0.5f) ||
                           (rideDirY_ > 0 && ly >= 0.5f) || (rideDirY_ < 0 && ly <= 0.5f);
            if (crossed) {
                rideDecidedX_ = cx;
                rideDecidedY_ = cy;
                p.x = static_cast<float>(cx) + 0.5f;
                p.y = static_cast<float>(cy) + 0.5f;
                auto railTo = [&](int dx, int dy) {
                    return state_->world.tileAt(cx + dx, cy + dy).terrain == core::Terrain::Rail;
                };
                if (!railTo(rideDirX_, rideDirY_)) {
                    // Corner or dead end: turn if exactly the track turns,
                    // never reverse.
                    static const int kDirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
                    int ndx = 0, ndy = 0;
                    for (const auto& d : kDirs) {
                        if (d[0] == -rideDirX_ && d[1] == -rideDirY_) continue;
                        if (railTo(d[0], d[1])) {
                            ndx = d[0];
                            ndy = d[1];
                            break;
                        }
                    }
                    if (ndx == 0 && ndy == 0) {
                        riding_ = false;
                        setStatus("End of the line!");
                    } else {
                        rideDirX_ = ndx;
                        rideDirY_ = ndy;
                    }
                }
                if (riding_) {
                    state_->facing = rideDirX_ > 0   ? core::Facing::Right
                                     : rideDirX_ < 0 ? core::Facing::Left
                                     : rideDirY_ < 0 ? core::Facing::Up
                                                     : core::Facing::Down;
                }
            }
        }
        moving_ = true;
        // Tab/build controls still work from the cart seat.
        if (input.lPressed) cycleHudTab(-1);
        if (input.rPressed) cycleHudTab(1);
        return;
    }

    core::Vec2f delta{0.0f, 0.0f};
    switch (input.move) {
        case platform::MoveDir::Up:
            delta = {0.0f, -1.0f};
            state_->facing = core::Facing::Up;
            break;
        case platform::MoveDir::Down:
            delta = {0.0f, 1.0f};
            state_->facing = core::Facing::Down;
            break;
        case platform::MoveDir::Left:
            delta = {-1.0f, 0.0f};
            state_->facing = core::Facing::Left;
            break;
        case platform::MoveDir::Right:
            delta = {1.0f, 0.0f};
            state_->facing = core::Facing::Right;
            break;
        case platform::MoveDir::None:
            break;
    }

    int32_t underX = static_cast<int32_t>(std::floor(state_->playerPos.x));
    int32_t underY = static_cast<int32_t>(std::floor(state_->playerPos.y));
    const core::Tile& under = state_->world.tileAt(underX, underY);
    bool snowUnder = core::biomeAt(state_->worldSeed, underX, underY) == core::Biome::Snow;
    bool onIce = under.terrain == core::Terrain::Water && snowUnder;
    // Open water (not frozen, not bridged) = swimming: slower, no work.
    bool wasSwimming = swimming_;
    swimming_ = under.terrain == core::Terrain::Water && !snowUnder &&
                under.placed != core::Placed::Bridge;
    if (swimming_ && !wasSwimming) platform::playSfx(platform::Sfx::Splash);
    float speed = kMoveTilesPerSecond;
    if (core::isPathTerrain(under.terrain)) speed *= kPathSpeedMul;
    if (swimming_) speed = 1.8f;

    if (onIce) {
        // Slippery: input only steers, momentum carries, letting go glides.
        constexpr float kIceAccel = 6.0f;
        constexpr float kIceFriction = 0.7f; // fraction of velocity lost per second
        velX_ += delta.x * kIceAccel * dt;
        velY_ += delta.y * kIceAccel * dt;
        if (delta.x == 0.0f && delta.y == 0.0f) {
            float f = 1.0f - kIceFriction * dt;
            velX_ *= f;
            velY_ *= f;
        }
        float mag = std::sqrt(velX_ * velX_ + velY_ * velY_);
        float vmax = speed * 1.35f; // a running start on ice outruns walking
        if (mag > vmax) {
            velX_ *= vmax / mag;
            velY_ *= vmax / mag;
        }
    } else {
        velX_ = delta.x * speed;
        velY_ = delta.y * speed;
    }

    // Per-axis movement: blocked on one axis slides along the other (and
    // on ice, hitting a wall kills that component of the glide). Water
    // never blocks the PLAYER anymore - stepping in means swimming (the
    // waterFrozen=true call makes water walkable while every other
    // obstacle still blocks).
    int64_t moveNow = core::nowSeconds();
    auto tryMove = [&](float dx, float dy) {
        core::Vec2f np{state_->playerPos.x + dx, state_->playerPos.y + dy};
        int32_t fx = static_cast<int32_t>(std::floor(state_->playerPos.x));
        int32_t fy = static_cast<int32_t>(std::floor(state_->playerPos.y));
        int32_t tx2 = static_cast<int32_t>(std::floor(np.x));
        int32_t ty2 = static_cast<int32_t>(std::floor(np.y));
        // The tile you already stand in can never block you - so if a
        // tree or plant regrows underfoot you simply step out of it.
        bool sameTile = tx2 == fx && ty2 == fy;
        if ((sameTile ||
             !core::blocksMovement(state_->world.tileAt(tx2, ty2), true, moveNow)) &&
            !cliffBlocked(fx, fy, tx2, ty2)) {
            state_->playerPos = np;
            return true;
        }
        return false;
    };
    if (velX_ != 0.0f && !tryMove(velX_ * dt, 0.0f)) velX_ = 0.0f;
    if (velY_ != 0.0f && !tryMove(0.0f, velY_ * dt)) velY_ = 0.0f;
    moving_ = std::fabs(velX_) + std::fabs(velY_) > 0.3f;

    // Stepping onto a door inside a building warps back out to just
    // south of it (any door in the room works - the stamped one or one
    // the player added).
    if (inInterior()) {
        int32_t dpx = static_cast<int32_t>(std::floor(state_->playerPos.x));
        int32_t dpy = static_cast<int32_t>(std::floor(state_->playerPos.y));
        if (state_->world.tileAt(dpx, dpy).placed == core::Placed::Door) {
            if (core::InteriorData* room = roomContaining(dpx, dpy)) {
                float ox = static_cast<float>(room->bx) + 0.5f;
                float oy = static_cast<float>(room->by) + 1.5f;
                // If something now blocks the doorstep, pop out beside it.
                if (core::blocksMovement(
                        state_->world.tileAt(room->bx, room->by + 1), true)) {
                    if (!core::blocksMovement(
                            state_->world.tileAt(room->bx + 1, room->by + 1), true)) {
                        ox += 1.0f;
                    } else if (!core::blocksMovement(
                                   state_->world.tileAt(room->bx - 1, room->by + 1), true)) {
                        ox -= 1.0f;
                    }
                }
                state_->playerPos = {ox, oy};
                state_->facing = core::Facing::Down;
                velX_ = velY_ = 0.0f;
                platform::playSfx(platform::Sfx::Chime);
            }
        }
    }

    // L/R always cycles the bottom-screen tab; X/Y cycles the Hammer's
    // loaded building (there's no separate Build tab - the ghost preview
    // on the tile you're facing IS the build UI).
    if (input.lPressed) cycleHudTab(-1);
    if (input.rPressed) cycleHudTab(1);
    if (buildModeActive()) {
        if (input.xPressed) cycleBuildGhost(-1);
        if (input.yPressed) cycleBuildGhost(1);
    }

    if (input.touchTapped) {
        switch (hudTab_) {
            case HudTab::Inventory: handleInventoryTap(input.touchX, input.touchY); break;
            case HudTab::Skills: break; // no interactive elements
        }
    }

    if (input.actionPressed) {
        if (swimming_) {
            setStatus("Can't work while swimming!");
        } else {
            doContextualAction();
        }
    }
    if (input.cancelPressed) {
        if (swimming_) {
            setStatus("Can't dig while swimming!");
        } else {
            doDigAction();
        }
    }
}

void WorldScene::handleInventoryTap(float x, float y) {
    // Drop/Trash buttons, below the grid+status area.
    if (y >= kDropBtnY && y < kDropBtnY + kDropBtnH) {
        if (x >= kDropBtnX && x < kDropBtnX + kDropBtnW) {
            dropSelected();
            return;
        }
        if (x >= kTrashBtnX && x < kTrashBtnX + kTrashBtnW) {
            trashSelected();
            return;
        }
    }

    // Tool bar, right under the header.
    if (y >= kToolBarY && y < kToolBarY + kToolBarH) {
        int idx = static_cast<int>((x - kToolBarX) / (kToolSlotPx + kToolBarGap));
        if (idx >= 0 && idx < core::kToolSlots) {
            if (idx == selectedTool_ || state_->toolBelt.slot(idx).item == core::kItemNone) {
                // Tapping the equipped tool again, or any empty tool slot,
                // unequips outright (matches the grid's same convention
                // below) rather than just clearing the tool-bar half.
                selectedTool_ = -1;
                selectedSlot_ = -1;
            } else {
                selectedTool_ = idx;
                selectedSlot_ = -1;
            }
        }
        return;
    }

    if (y < kInvGridY || y >= kInvGridY + kInvRows * kSlotPx) return;
    if (x < kInvGridX) return;
    int col = static_cast<int>((x - kInvGridX) / kSlotPx);
    int row = static_cast<int>((y - kInvGridY) / kSlotPx);
    int idx = row * kInvCols + col;
    if (col < 0 || col >= kInvCols || idx < 0 || idx >= core::Inventory::slotCount()) return;

    if (idx == selectedSlot_ || state_->inventory.slot(idx).item == core::kItemNone) {
        // Tapping the already-equipped slot again, or any empty slot,
        // unequips - "empty hands" for toolless work (till/harvest/forage
        // all already work with nothing selected).
        selectedSlot_ = -1;
        selectedTool_ = -1;
    } else {
        selectedSlot_ = idx;
        selectedTool_ = -1;
    }
}

void WorldScene::handleCraftInput(const platform::InputState& input) {
    if (input.cancelPressed || input.menuPressed) {
        craftOpen_ = false;
        return;
    }
    if (input.upPressed) craftSel_ = (craftSel_ + kRecipeCount - 1) % kRecipeCount;
    if (input.downPressed) craftSel_ = (craftSel_ + 1) % kRecipeCount;
    if (!input.confirmPressed) return;

    const Recipe& r = kRecipes[craftSel_];
    int owned = state_->toolBelt.countOf(r.item);
    if (owned > 0) {
        setStatus("Already have a %s", core::kItemTable[r.item].name);
        platform::playSfx(platform::Sfx::Deny);
        return;
    }
    if (!canAffordCost(r.cost)) {
        if (r.cost.stone > 0) {
            setStatus("Need %d Wood %d Stone", r.cost.wood, r.cost.stone);
        } else {
            setStatus("Need %d Wood", r.cost.wood);
        }
        platform::playSfx(platform::Sfx::Deny);
        return;
    }
    if (r.cost.wood > 0) state_->inventory.remove(core::kItemWood, r.cost.wood);
    if (r.cost.stone > 0) state_->inventory.remove(core::kItemStone, r.cost.stone);
    // Its own fixed tool-bar slot - unlike a general item, crafting a tool
    // can never fail for lack of room.
    state_->toolBelt.add(r.item);
    setStatus("Crafted %s!", core::kItemTable[r.item].name);
    platform::playSfx(platform::Sfx::Harvest);
}

void WorldScene::drawCraftUi(const platform::Renderer& renderer) const {
    renderer.beginBottom(C2D_Color32(0x20, 0x28, 0x18, 0xFF));
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, kBottomW, 24.0f, C2D_Color32(0x14, 0x1a, 0x0e, 0xFF));
    renderer.drawTextFlat("WORKBENCH", 8.0f, 5.0f, 0.5f, C2D_Color32(0xE8, 0xE8, 0xC0, 0xFF));

    for (int i = 0; i < kRecipeCount; i++) {
        const Recipe& r = kRecipes[i];
        float y = 32.0f + static_cast<float>(i) * 34.0f;
        if (i == craftSel_) {
            C2D_DrawRectSolid(2.0f, y - 2.0f, 0.0f, kBottomW - 4.0f, 32.0f,
                              C2D_Color32(0x50, 0x60, 0x30, 0xFF));
        }
        renderer.drawSpriteFlat(spriteForItem(r.item), 8.0f, y + 3.0f, 1.4f);
        int owned = state_->toolBelt.countOf(r.item);
        bool affordable = state_->inventory.countOf(core::kItemWood) >= r.cost.wood &&
                          state_->inventory.countOf(core::kItemStone) >= r.cost.stone;
        uint32_t nameCol = owned ? C2D_Color32(0x80, 0x88, 0x70, 0xFF)
                                 : C2D_Color32(0xFF, 0xF0, 0xB0, 0xFF);
        renderer.drawTextFlat(core::kItemTable[r.item].name, 40.0f, y + 2.0f, 0.5f, nameCol);
        char costBuf[32];
        if (owned) {
            snprintf(costBuf, sizeof(costBuf), "Owned");
        } else if (r.cost.stone > 0) {
            snprintf(costBuf, sizeof(costBuf), "%d Wood  %d Stone", r.cost.wood, r.cost.stone);
        } else {
            snprintf(costBuf, sizeof(costBuf), "%d Wood", r.cost.wood);
        }
        renderer.drawTextFlat(costBuf, 40.0f, y + 17.0f, 0.36f,
                              owned ? C2D_Color32(0x80, 0x88, 0x70, 0xFF)
                              : affordable ? C2D_Color32(0xB8, 0xE0, 0x90, 0xFF)
                                           : C2D_Color32(0xC8, 0x70, 0x70, 0xFF));
    }

    if (statusFrames_ > 0) {
        renderer.drawTextFlat(statusMsg_, 8.0f, 206.0f, 0.4f, C2D_Color32(0xFF, 0xE8, 0x80, 0xFF));
    }
    renderer.drawTextFlat("UP/DOWN choose   A craft   B close", 8.0f, 224.0f, 0.38f,
                          C2D_Color32(0x90, 0x98, 0x80, 0xFF));
}

void WorldScene::handlePauseInput(const platform::InputState& input) {
    if (input.upPressed) {
        pauseSelection_ = (pauseSelection_ + kPauseOptionCount - 1) % kPauseOptionCount;
    } else if (input.downPressed) {
        pauseSelection_ = (pauseSelection_ + 1) % kPauseOptionCount;
    }

    if (input.cancelPressed || input.menuPressed) {
        paused_ = false;
        return;
    }

    if (input.confirmPressed) {
        switch (pauseSelection_) {
            case 0:
                platform::saveToDisk(*state_);
                setStatus("Saved");
                break;
            case 1:
                teleportHome();
                break;
            default:
                break; // Resume - just closes the menu
        }
        paused_ = false;
    }
}

void WorldScene::handleChestInput(const platform::InputState& input) {
    if (input.cancelPressed || input.menuPressed) {
        chestOpen_ = false;
        return;
    }
    if (!input.touchTapped) return;

    core::ChestData* chest = state_->chestAt(chestX_, chestY_);
    if (!chest) { // demolished out from under us somehow
        chestOpen_ = false;
        return;
    }

    int col = static_cast<int>((input.touchX - kChestGridX) / kChestSlotPx);
    if (col < 0 || col >= kInvCols) return;

    auto rowAt = [&](float startY) {
        float rel = input.touchY - startY;
        if (rel < 0 || rel >= kInvRows * kChestSlotPx) return -1;
        return static_cast<int>(rel / kChestSlotPx);
    };

    int chestRow = rowAt(kChestTopY);
    int invRow = rowAt(kChestBotY);
    if (chestRow >= 0) {
        int idx = chestRow * kInvCols + col;
        const core::ItemStack& s = chest->items.slot(idx);
        if (s.item == core::kItemNone) return;
        int moved = state_->inventory.add(s.item, s.count);
        chest->items.setSlot(idx, moved == s.count ? core::kItemNone : s.item,
                             static_cast<uint16_t>(s.count - moved));
    } else if (invRow >= 0) {
        int idx = invRow * kInvCols + col;
        const core::ItemStack& s = state_->inventory.slot(idx);
        if (s.item == core::kItemNone) return;
        int moved = chest->items.add(s.item, s.count);
        state_->inventory.setSlot(idx, moved == s.count ? core::kItemNone : s.item,
                                  static_cast<uint16_t>(s.count - moved));
    }
}

void WorldScene::teleportHome() {
    if (mineFloor_ > 0) exitMine("Climbed out of the mine.");
    if (state_->hasLastFieldPos) {
        state_->playerPos = state_->lastFieldPos;
        state_->hasLastFieldPos = false;
        return;
    }

    state_->lastFieldPos = state_->playerPos;
    state_->hasLastFieldPos = true;

    if (state_->home.set) {
        // Arrive on the tile *below* the Camp - the camp itself blocks.
        state_->playerPos = {static_cast<float>(state_->home.x) + 0.5f,
                             static_cast<float>(state_->home.y) + 1.5f};
    } else {
        state_->playerPos = {0.5f, 0.5f}; // spawn - no Camp placed yet
    }
}

core::Vec2f WorldScene::facingOffset() const {
    switch (state_->facing) {
        case core::Facing::Up: return {0.0f, -1.0f};
        case core::Facing::Down: return {0.0f, 1.0f};
        case core::Facing::Left: return {-1.0f, 0.0f};
        case core::Facing::Right: return {1.0f, 0.0f};
    }
    return {0.0f, 0.0f};
}

// --- The contextual A-button chain -----------------------------------------

void WorldScene::gatherAt(core::Tile& tile, int32_t tx, int32_t ty, core::Decoration kind,
                          const core::NodeBalance& bal, int64_t now) {
    int tier = clampTier(tile.decoTier);
    int level = state_->skillLevel(bal.skill);
    core::GatherResult result = core::gatherNode(tile, kind, level, now);
    switch (result) {
        case core::GatherResult::NotANode:
            return;
        case core::GatherResult::Regrowing:
            setStatus("Still regrowing...");
            return;
        case core::GatherResult::LevelTooLow:
            setStatus("Need %s Lv %d", core::kSkillNames[static_cast<int>(bal.skill)],
                      bal.levelReq[tier]);
            platform::playSfx(platform::Sfx::Deny);
            return;
        case core::GatherResult::Ok:
            break;
    }

    state_->world.markDirty(tx, ty);
    actionTimer_ = 20;
    int yield = bal.baseYield[tier] + core::yieldBonus(level);
    char extra[32] = {0};

    switch (kind) {
        case core::Decoration::Tree:
            state_->inventory.add(core::kItemWood, yield);
            if (tier == 2 && rollPct(core::kAppleChancePct)) {
                // Fruit trees shake loose THEIR fruit when chopped - the
                // same per-tile hash that picked the tree art picks the
                // drop (apple/orange/pear/peach).
                static const core::ItemId kFruit[4] = {core::kItemApple, core::kItemOrange,
                                                       core::kItemPear, core::kItemPeach};
                core::ItemId fruit = kFruit[visHash(tx, ty) % 4];
                state_->inventory.add(fruit, 1);
                snprintf(extra, sizeof(extra), " +1 %s", core::kItemTable[fruit].name);
            } else if (rollPct(core::kSaplingChancePct)) {
                state_->inventory.add(core::kItemSapling, 1);
                snprintf(extra, sizeof(extra), " +1 Sapling");
            }
            setStatus("+%d Wood%s", yield, extra);
            platform::playSfx(platform::Sfx::Chop);
            // The felled tree bursts into leaves (premium fall animation).
            poofs_.push_back({tx, ty, 0.0f});
            break;
        case core::Decoration::Rock:
            state_->inventory.add(core::kItemStone, yield);
            if (rollPct(core::kOreChancePct[tier])) {
                state_->inventory.add(core::kItemOre, 1);
                snprintf(extra, sizeof(extra), " +1 Ore");
            }
            // Rocks don't grow back - mined is gone. Fresh ones turn up
            // on their own now and then (see the spawn sweep in update).
            tile.decoration = core::Decoration::None;
            tile.depleted = false;
            setStatus("+%d Stone%s", yield, extra);
            platform::playSfx(platform::Sfx::Mine);
            break;
        case core::Decoration::Bush: {
            state_->inventory.add(core::kItemBerries, yield);
            if (rollPct(core::kBushSeedChancePct)) {
                // Higher-tier bushes teach the better crops (see the seed
                // pools in balance.h - 12 species total).
                static const core::ItemId* kPools[3] = {core::kSeedPoolT0, core::kSeedPoolT1,
                                                        core::kSeedPoolT2};
                core::ItemId seed =
                    kPools[tier][nextRand() % static_cast<uint32_t>(core::kSeedPoolSizes[tier])];
                state_->inventory.add(seed, 1);
                snprintf(extra, sizeof(extra), " +1 %s", core::kItemTable[seed].name);
            }
            setStatus("+%d Berries%s", yield, extra);
            platform::playSfx(platform::Sfx::Harvest);
            break;
        }
        case core::Decoration::Mushroom:
            state_->inventory.add(core::kItemMushroom, yield);
            setStatus("+%d Mushroom", yield);
            platform::playSfx(platform::Sfx::Harvest);
            break;
        case core::Decoration::WildPumpkin:
        case core::Decoration::WildSunflower: {
            // Wild patches yield the crop itself, sometimes with a seed
            // so a found patch can seed a real field back home.
            bool pumpkin = kind == core::Decoration::WildPumpkin;
            core::ItemId cropItem = pumpkin ? core::kItemPumpkin : core::kItemSunflower;
            state_->inventory.add(cropItem, yield);
            if (rollPct(core::kWildPatchSeedChancePct)) {
                core::ItemId seed =
                    pumpkin ? core::kItemPumpkinSeed : core::kItemSunflowerSeed;
                state_->inventory.add(seed, 1);
                snprintf(extra, sizeof(extra), " +1 %s", core::kItemTable[seed].name);
            }
            setStatus("+%d %s%s", yield, core::kItemTable[cropItem].name, extra);
            platform::playSfx(platform::Sfx::Harvest);
            break;
        }
        default:
            break;
    }
    awardXp(bal.skill, bal.xp[tier]);
}

bool WorldScene::tryTame(WildAnimal& wild, int64_t now) {
    if (wild.kind == 2) { // frog
        setStatus("Ribbit! (frogs are free spirits)");
        return false;
    }
    if (wild.kind == 3) { // ambient fish
        setStatus("It darts away! (try the Fishing Rod)");
        return false;
    }

    const core::ItemStack& sel = selectedStack();
    bool isChicken = wild.kind == 0;

    // A refused animal says so itself: the request pops up as a speech
    // bubble over its head (drawn in the wild-animal pass) with the food
    // it wants; the status line keeps the full wording.
    if (isChicken && sel.item != core::kItemBerries) {
        setStatus("Chickens want Berries!");
        wild.reqT = kReqBubbleFrames;
        return false;
    }
    bool fruit = sel.item == core::kItemApple || sel.item == core::kItemOrange ||
                 sel.item == core::kItemPear || sel.item == core::kItemPeach;
    if (!isChicken && sel.item != core::kItemHay && sel.item != core::kItemTurnip && !fruit) {
        setStatus("Cows want Hay, Turnips or fruit!");
        wild.reqT = kReqBubbleFrames;
        return false;
    }

    int herdingLv = state_->skillLevel(core::Skill::Herding);
    if (static_cast<int>(state_->animals.size()) >= core::tameCapacity(herdingLv)) {
        setStatus("Herd full - level up Herding!");
        return false;
    }

    // Find the nearest coop/barn (within ~16 tiles of the player) with room.
    core::Placed wanted = isChicken ? core::Placed::Coop : core::Placed::Barn;
    int capacity = isChicken ? core::kCoopCapacity : core::kBarnCapacity;
    int32_t px = static_cast<int32_t>(std::floor(state_->playerPos.x));
    int32_t py = static_cast<int32_t>(std::floor(state_->playerPos.y));
    int32_t bestX = 0, bestY = 0;
    int32_t bestDist = INT32_MAX;
    for (int32_t sy = py - 16; sy <= py + 16; sy++) {
        for (int32_t sx = px - 16; sx <= px + 16; sx++) {
            if (state_->world.tileAt(sx, sy).placed != wanted) continue;
            int residents = 0;
            for (const core::TamedAnimal& a : state_->animals) {
                if (a.homeX == sx && a.homeY == sy) residents++;
            }
            if (residents >= capacity) continue;
            int32_t d = (sx - px) * (sx - px) + (sy - py) * (sy - py);
            if (d < bestDist) {
                bestDist = d;
                bestX = sx;
                bestY = sy;
            }
        }
    }
    if (bestDist == INT32_MAX) {
        setStatus(isChicken ? "Need a Coop nearby" : "Need a Barn nearby");
        return false;
    }

    // Consume the food.
    if (isChicken) {
        if (!state_->inventory.remove(core::kItemBerries, core::kTameChickenBerries)) {
            setStatus("Need %d Berries", core::kTameChickenBerries);
            return false;
        }
    } else if (sel.item == core::kItemHay) {
        if (!state_->inventory.remove(core::kItemHay, core::kTameCowHay)) {
            setStatus("Need %d Hay", core::kTameCowHay);
            return false;
        }
    } else if (fruit) {
        if (!state_->inventory.remove(sel.item, core::kTameCowApples)) {
            setStatus("Need %d %ss", core::kTameCowApples, core::kItemTable[sel.item].name);
            return false;
        }
    } else {
        if (!state_->inventory.remove(core::kItemTurnip, core::kTameCowTurnips)) {
            setStatus("Need %d Turnips", core::kTameCowTurnips);
            return false;
        }
    }

    core::TamedAnimal tamed;
    tamed.species = isChicken ? core::AnimalSpecies::Chicken : core::AnimalSpecies::Cow;
    tamed.variant = wild.variant;
    tamed.homeX = bestX;
    tamed.homeY = bestY;
    tamed.tamedAt = now; // it's a baby now; grows up on kBabyGrowSec
    tamed.lastCollectedAt = now;
    state_->animals.push_back(tamed);
    awardXp(core::Skill::Herding, core::kXpTame);
    setStatus("Tamed! The baby moved in.");
    platform::playSfx(platform::Sfx::Tame);
    emoteSprite_ = atlas_emote_heart_idx;
    emoteExtra_ = -1;
    emoteT_ = 110;
    return true;
}

void WorldScene::collectProduce(int32_t tx, int32_t ty, core::Placed building, int64_t now) {
    bool coop = building == core::Placed::Coop;
    int32_t interval = coop ? core::kEggIntervalSec : core::kMilkIntervalSec;
    core::ItemId product = coop ? core::kItemEgg : core::kItemMilk;

    int collected = 0;
    bool anyBaby = false;
    for (core::TamedAnimal& a : state_->animals) {
        if (a.homeX != tx || a.homeY != ty) continue;
        if (!core::elapsedAtLeast(a.tamedAt, now, core::kBabyGrowSec)) {
            anyBaby = true; // still growing up - no produce yet
            continue;
        }
        if (core::elapsedAtLeast(a.lastCollectedAt, now, interval)) {
            if (state_->inventory.add(product, 1) == 0) break; // full
            a.lastCollectedAt = now;
            collected++;
        }
    }
    if (collected > 0) {
        awardXp(core::Skill::Herding, core::kXpCollect * collected);
        setStatus("+%d %s", collected, core::kItemTable[product].name);
        platform::playSfx(platform::Sfx::Harvest);
        // Happy herd! (premium mood icons, shown over the player's head)
        emoteSprite_ = atlas_ui_happy_idx;
        emoteExtra_ = -1;
        emoteT_ = 70;
    } else if (anyBaby) {
        setStatus("Still growing up...");
    } else {
        setStatus(coop ? "No eggs yet" : "No milk yet");
        emoteSprite_ = atlas_ui_sad_idx;
        emoteExtra_ = -1;
        emoteT_ = 70;
    }
}

void WorldScene::collectHoney(int32_t tx, int32_t ty, int64_t now) {
    core::HiveData* hive = state_->hiveAt(tx, ty);
    if (!hive) return;
    if (core::elapsedAtLeast(hive->lastCollectedAt, now, core::kHoneyIntervalSec)) {
        if (state_->inventory.add(core::kItemHoney, 1) == 0) {
            setStatus("Inventory full!");
            platform::playSfx(platform::Sfx::Deny);
            return;
        }
        hive->lastCollectedAt = now;
        awardXp(core::Skill::Foraging, core::kXpHoney);
        setStatus("+1 Honey");
        platform::playSfx(platform::Sfx::Harvest);
    } else {
        setStatus("The bees are still working...");
    }
}

bool WorldScene::cliffBlocked(int32_t fromX, int32_t fromY, int32_t toX, int32_t toY) const {
    // No terrain elevation indoors (the band's phantom hills are void).
    if (fromY > kInteriorViewY) return false;
    bool fromE = core::elevAt(state_->worldSeed, fromX, fromY);
    bool toE = core::elevAt(state_->worldSeed, toX, toY);
    if (fromE == toE) return false;
    int32_t hx = fromE ? fromX : toX;
    int32_t hy = fromE ? fromY : toY;
    int32_t lx = fromE ? toX : fromX;
    int32_t ly = fromE ? toY : fromY;
    // Only a stair ramp lets you cross the rim: the high tile must be a
    // STRAIGHT south-facing edge (raised on its N/E/W sides - matching
    // the drawn stairs, which replace that rim piece) with rampAt set.
    uint32_t seed = state_->worldSeed;
    return !(lx == hx && ly == hy + 1 && core::rampAt(seed, hx, hy) &&
             core::elevAt(seed, hx, hy - 1) && core::elevAt(seed, hx + 1, hy) &&
             core::elevAt(seed, hx - 1, hy));
}

// --- Building interiors ------------------------------------------------------

bool WorldScene::inInterior() const {
    return state_ && state_->playerPos.y > static_cast<float>(kInteriorViewY);
}

void WorldScene::interiorAnchor(int32_t bx, int32_t by, int32_t* ax, int32_t* ay) {
    // 48-tile spacing per building coordinate keeps even adjacent
    // buildings' rooms (max width ~31 tiles) from ever overlapping.
    *ax = bx * 48;
    *ay = kInteriorBandY + by * 48;
}

void WorldScene::interiorSizeFor(core::Placed kind, uint8_t* wl, uint8_t* wr, uint8_t* h) {
    switch (kind) {
        case core::Placed::Camp: *wl = *wr = 2; *h = 4; break;    // cozy tent
        case core::Placed::Coop: *wl = *wr = 2; *h = 5; break;    // 5x5
        case core::Placed::Barn: *wl = *wr = 3; *h = 5; break;    // 7x5
        case core::Placed::Hut: *wl = *wr = 3; *h = 5; break;
        case core::Placed::Cottage: *wl = *wr = 4; *h = 6; break; // 9x6
        case core::Placed::Manor: *wl = *wr = 5; *h = 7; break;   // 11x7
        default: *wl = *wr = 0; *h = 0; break;
    }
}

void WorldScene::stampInterior(const core::InteriorData& room) {
    int32_t ax, ay;
    interiorAnchor(room.bx, room.by, &ax, &ay);
    int32_t x0 = ax - room.wl, x1 = ax + room.wr;
    int32_t y0 = ay, y1 = ay + room.h - 1;
    for (int32_t ty = y0; ty <= y1; ty++) {
        for (int32_t tx = x0; tx <= x1; tx++) {
            core::Tile& t = state_->world.tileAt(tx, ty);
            t.terrain = core::Terrain::Floor;
            t.tilled = false;
            t.hasCrop = false;
            t.watered = false;
            t.decoration = core::Decoration::None;
            t.depleted = false;
            bool ring = tx == x0 || tx == x1 || ty == y0 || ty == y1;
            if (ring) {
                // Door in the south wall at the anchor column.
                t.placed = (tx == ax && ty == y1) ? core::Placed::Door : core::Placed::Wall;
                t.decoTier = 0;
            } else if (t.placed == core::Placed::Wall || t.placed == core::Placed::Door) {
                // Annexed a former wall line (room expansion) - clear it;
                // real furniture on interior tiles is left alone.
                t.placed = core::Placed::None;
            }
            state_->world.markDirty(tx, ty);
        }
    }
}

void WorldScene::enterBuilding(int32_t bx, int32_t by, core::Placed kind) {
    uint8_t wl, wr, h;
    interiorSizeFor(kind, &wl, &wr, &h);
    if (h == 0) return;
    core::InteriorData* room = state_->interiorAt(bx, by);
    if (!room) {
        core::InteriorData in;
        in.bx = bx;
        in.by = by;
        in.kind = static_cast<uint8_t>(kind);
        in.wl = wl;
        in.wr = wr;
        in.h = h;
        state_->interiors.push_back(in);
        room = &state_->interiors.back();
        stampInterior(*room);
    } else if (room->kind != static_cast<uint8_t>(kind)) {
        // The tile was rebuilt as a different building: fresh room.
        room->kind = static_cast<uint8_t>(kind);
        room->wl = wl;
        room->wr = wr;
        room->h = h;
        stampInterior(*room);
    }
    int32_t ax, ay;
    interiorAnchor(bx, by, &ax, &ay);
    state_->playerPos = {static_cast<float>(ax) + 0.5f,
                         static_cast<float>(ay + room->h - 2) + 0.5f};
    state_->facing = core::Facing::Up;
    riding_ = false;
    fishState_ = 0;
    swimming_ = false;
    platform::playSfx(platform::Sfx::Chime);
    // Coops and barns hand over their produce as you step inside.
    if (kind == core::Placed::Coop || kind == core::Placed::Barn) {
        collectProduce(bx, by, kind, core::nowSeconds());
    } else {
        setStatus("Home sweet home.");
    }
}

int WorldScene::wildStageAt(const core::Tile& tile, int32_t tx, int32_t ty, int64_t now) const {
    if (!core::nodeReady(tile, now)) {
        // Regrowing after a pick: sprout, then half-grown, on the
        // respawn window.
        int64_t el = core::elapsedSeconds(tile.timestamp, now);
        return el * 2 >= core::kWildPatchBalance.respawnSec[0] ? 2 : 1;
    }
    // Staggered wild cycle: at any moment most plants are ripe, with a
    // scattering of younger ones working their way up.
    uint32_t ph = (static_cast<uint32_t>(now / core::kWildGrowSec) + visHash(tx, ty)) % 8;
    return ph == 0 ? 1 : ph == 1 ? 2 : 3;
}

core::InteriorData* WorldScene::roomContaining(int32_t tx, int32_t ty) const {
    for (const core::InteriorData& in : state_->interiors) {
        int32_t ax, ay;
        interiorAnchor(in.bx, in.by, &ax, &ay);
        if (tx >= ax - in.wl && tx <= ax + in.wr && ty >= ay && ty <= ay + in.h - 1) {
            return const_cast<core::InteriorData*>(&in);
        }
    }
    return nullptr;
}

bool WorldScene::canPlaceTerrain(core::ItemId item, int32_t tx, int32_t ty) const {
    const core::Tile& tile = state_->world.tileAt(tx, ty);
    bool legal = (item == core::kItemBridge || item == core::kItemBoat)
                     ? core::canPlaceBridge(tile)
                     : core::canPlace(tile);
    if (!legal) return false;

    // No buildings-within-buildings - their rooms would nest without end.
    if (ty > kInteriorViewY &&
        (item == core::kItemCamp || item == core::kItemCoop || item == core::kItemBarn ||
         item == core::kItemCottage || item == core::kItemHut || item == core::kItemManor)) {
        return false;
    }

    int32_t px = static_cast<int32_t>(std::floor(state_->playerPos.x));
    int32_t py = static_cast<int32_t>(std::floor(state_->playerPos.y));
    bool walkable = item == core::kItemBridge || item == core::kItemPath ||
                    item == core::kItemPathDirt || item == core::kItemPathPlank ||
                    item == core::kItemRail || item == core::kItemRug ||
                    item == core::kItemRugLong || item == core::kItemFloor ||
                    item == core::kItemGate ||
                    item == core::kItemGateRight || item == core::kItemDoor;
    if (!walkable && tx == px && ty == py) {
        return false; // don't entomb yourself
    }
    return true;
}

bool WorldScene::canPlaceGhost(core::ItemId item, int32_t tx, int32_t ty) const {
    return canPlaceTerrain(item, tx, ty) && state_->inventory.countOf(item) > 0;
}

bool WorldScene::canAffordCost(const core::BuildCost& cost) const {
    return state_->inventory.countOf(core::kItemWood) >= cost.wood &&
           state_->inventory.countOf(core::kItemStone) >= cost.stone;
}

bool WorldScene::applyPlacement(core::Tile& tile, int32_t tx, int32_t ty, core::ItemId item) {
    // Placed tiles never carry a resource node, so decoTier is free to
    // store the furniture variant (facing/color/wood) - start at 0; A on
    // the placed piece cycles it.
    tile.decoTier = 0;
    switch (item) {
        case core::kItemFence: tile.placed = core::Placed::Fence; break;
        case core::kItemBridge: tile.placed = core::Placed::Bridge; break;
        case core::kItemPath: tile.terrain = core::Terrain::Path; break;
        case core::kItemPathDirt: tile.terrain = core::Terrain::PathDirt; break;
        case core::kItemPathPlank: tile.terrain = core::Terrain::PathPlank; break;
        case core::kItemRail: tile.terrain = core::Terrain::Rail; break;
        case core::kItemCamp:
            tile.placed = core::Placed::Camp;
            state_->home.set = true;
            state_->home.x = tx;
            state_->home.y = ty;
            setStatus("Home set!");
            break;
        case core::kItemCoop: tile.placed = core::Placed::Coop; break;
        case core::kItemBarn: tile.placed = core::Placed::Barn; break;
        case core::kItemChest: {
            tile.placed = core::Placed::Chest;
            core::ChestData chest;
            chest.x = tx;
            chest.y = ty;
            state_->chests.push_back(chest);
            break;
        }
        case core::kItemLamp: tile.placed = core::Placed::Lamp; break;
        case core::kItemChair: tile.placed = core::Placed::Chair; break;
        case core::kItemRug: tile.placed = core::Placed::Rug; break;
        case core::kItemRugLong: tile.placed = core::Placed::RugLong; break;
        case core::kItemBed: tile.placed = core::Placed::Bed; break;
        case core::kItemWoodTable: tile.placed = core::Placed::Table; break;
        case core::kItemDresser: tile.placed = core::Placed::Dresser; break;
        case core::kItemStool: tile.placed = core::Placed::Stool; break;
        case core::kItemBench: tile.placed = core::Placed::Bench; break;
        case core::kItemWorkbench: tile.placed = core::Placed::Workbench; break;
        case core::kItemGate: tile.placed = core::Placed::Gate; break;
        case core::kItemGateRight: tile.placed = core::Placed::GateRight; break;
        case core::kItemWell: tile.placed = core::Placed::Well; break;
        case core::kItemWall: tile.placed = core::Placed::Wall; break;
        case core::kItemDoor: tile.placed = core::Placed::Door; break;
        case core::kItemRoof: tile.placed = core::Placed::Roof; break;
        case core::kItemFloor: tile.terrain = core::Terrain::Floor; break;
        case core::kItemXmasTree: tile.placed = core::Placed::XmasTree; break;
        case core::kItemBeehive: {
            tile.placed = core::Placed::Beehive;
            core::HiveData hive;
            hive.x = tx;
            hive.y = ty;
            hive.lastCollectedAt = core::nowSeconds();
            state_->hives.push_back(hive);
            break;
        }
        case core::kItemCampfire: tile.placed = core::Placed::Campfire; break;
        case core::kItemSign: tile.placed = core::Placed::Sign; break;
        case core::kItemMailbox: tile.placed = core::Placed::Mailbox; break;
        case core::kItemMineShaft: tile.placed = core::Placed::MineShaft; break;
        case core::kItemCottage: tile.placed = core::Placed::Cottage; break;
        case core::kItemHut: tile.placed = core::Placed::Hut; break;
        case core::kItemManor: tile.placed = core::Placed::Manor; break;
        case core::kItemSnowman: tile.placed = core::Placed::Snowman; break;
        case core::kItemClone:
            // Not a tile at all: the mirror shatters and the clone steps
            // out as a living entity on this spot.
            state_->clone.exists = true;
            state_->clone.pos = {static_cast<float>(tx) + 0.5f, static_cast<float>(ty) + 0.5f};
            state_->clone.task = 0;
            cloneHasTarget_ = false;
            setStatus("The mirror shatters - your clone steps out!");
            platform::playSfx(platform::Sfx::LevelUp);
            return true;
        case core::kItemTrough: tile.placed = core::Placed::Trough; break;
        case core::kItemHayBale: tile.placed = core::Placed::HayBale; break;
        case core::kItemWaterTray:
            tile.placed = core::Placed::WaterTray;
            tile.decoTier = 2; // starts empty; a full watering can fills it
            break;
        case core::kItemBoat: tile.placed = core::Placed::Boat; break;
        case core::kItemPicnic: tile.placed = core::Placed::Picnic; break;
        case core::kItemPresent: tile.placed = core::Placed::Present; break;
        // Chest upgrade tiers: same chest storage, fancier skin (the
        // decoTier values past the 4 biome woods select silver/gold art).
        case core::kItemChestSilver:
        case core::kItemChestGold: {
            tile.placed = core::Placed::Chest;
            tile.decoTier = item == core::kItemChestSilver ? 4 : 5;
            core::ChestData chest;
            chest.x = tx;
            chest.y = ty;
            state_->chests.push_back(chest);
            break;
        }
        default: return false;
    }
    state_->world.markDirty(tx, ty);
    platform::playSfx(platform::Sfx::Place);
    return true;
}

bool WorldScene::placeSelected(core::Tile& tile, int32_t tx, int32_t ty, core::ItemId item) {
    if (!canPlaceGhost(item, tx, ty)) return false;
    if (!applyPlacement(tile, tx, ty, item)) return false;
    state_->inventory.remove(item, 1);
    return true;
}

bool WorldScene::buildFromMaterials(core::Tile& tile, int32_t tx, int32_t ty, core::ItemId item,
                                     const core::BuildCost& cost) {
    // The Clone Mirror also demands its specialty crystals (and there
    // can only ever be one clone).
    static const core::ItemId kGems[4] = {core::kItemRuby, core::kItemDiamond,
                                          core::kItemEmerald, core::kItemAmethyst};
    if (item == core::kItemClone) {
        if (state_->clone.exists) {
            setStatus("Your clone already exists!");
            platform::playSfx(platform::Sfx::Deny);
            return false;
        }
        for (core::ItemId gem : kGems) {
            if (state_->inventory.countOf(gem) < core::kCloneGemCount) {
                setStatus("Needs %d of EACH gem (ruby/diamond/emerald/amethyst)",
                          core::kCloneGemCount);
                platform::playSfx(platform::Sfx::Deny);
                return false;
            }
        }
    }
    if (!canPlaceTerrain(item, tx, ty) || !canAffordCost(cost)) return false;
    if (!applyPlacement(tile, tx, ty, item)) return false;
    if (cost.wood > 0) state_->inventory.remove(core::kItemWood, cost.wood);
    if (cost.stone > 0) state_->inventory.remove(core::kItemStone, cost.stone);
    if (item == core::kItemClone) {
        for (core::ItemId gem : kGems) state_->inventory.remove(gem, core::kCloneGemCount);
    }
    // Construction teaches construction - bigger builds teach more.
    awardXp(core::Skill::Building, core::kXpBuildPerMaterial * (cost.wood + cost.stone));
    return true;
}

void WorldScene::doContextualAction() {
    core::Vec2f offset = facingOffset();
    int32_t tx = static_cast<int32_t>(std::floor(state_->playerPos.x + offset.x));
    int32_t ty = static_cast<int32_t>(std::floor(state_->playerPos.y + offset.y));

    core::Tile& tile = state_->world.tileAt(tx, ty);
    int64_t now = core::nowSeconds();
    const core::ItemStack& selected = selectedStack();

    // 0. A dropped item stack sitting on the faced tile: pick it up first,
    // regardless of anything else there.
    if (core::GroundItem* ground = state_->groundItemAt(tx, ty)) {
        int added = state_->inventory.add(ground->item, ground->count);
        if (added > 0) {
            setStatus("Picked up %d %s", added, core::kItemTable[ground->item].name);
            platform::playSfx(platform::Sfx::Ui);
            ground->count -= static_cast<uint16_t>(added);
            if (ground->count == 0) {
                for (size_t i = 0; i < state_->groundItems.size(); i++) {
                    if (&state_->groundItems[i] == ground) {
                        state_->groundItems.erase(state_->groundItems.begin() + static_cast<long>(i));
                        break;
                    }
                }
            }
        } else {
            setStatus("Inventory full!");
            platform::playSfx(platform::Sfx::Deny);
        }
        return;
    }

    // 1. Placed-building interactions.
    if (tile.placed == core::Placed::MineShaft) {
        enterMine();
        return;
    }
    if (tile.placed == core::Placed::Chest) {
        chestOpen_ = true;
        chestX_ = tx;
        chestY_ = ty;
        platform::playSfx(platform::Sfx::Ui);
        return;
    }
    // Buildings with interiors: A steps inside (coops/barns auto-collect
    // their produce as you walk in).
    if (tile.placed == core::Placed::Coop || tile.placed == core::Placed::Barn ||
        tile.placed == core::Placed::Camp || tile.placed == core::Placed::Cottage ||
        tile.placed == core::Placed::Hut || tile.placed == core::Placed::Manor) {
        enterBuilding(tx, ty, tile.placed);
        return;
    }
    if (tile.placed == core::Placed::Beehive) {
        collectHoney(tx, ty, now);
        return;
    }
    if (tile.placed == core::Placed::Workbench) {
        craftOpen_ = true;
        craftSel_ = 0;
        platform::playSfx(platform::Sfx::Ui);
        return;
    }
    // Sleeping: A on a Bed at night (20:00-6:00) fast-forwards the world
    // to 6am - day/night, weather, crops, respawns, and animal timers all
    // just see a later "now" (the offset persists in the save, and full
    // hearts too). Daytime beds fall through to the restyle below.
    if (tile.placed == core::Placed::Bed) {
        float h = core::dayHour(now);
        if (h >= 20.0f || h < 6.0f) {
            float hoursLeft = h >= 20.0f ? 30.0f - h : 6.0f - h;
            state_->clockOffset +=
                static_cast<int64_t>(hoursLeft / 24.0f * static_cast<float>(core::kDayLengthSec)) + 1;
            core::setClockOffset(state_->clockOffset);
            hp_ = core::kMaxHp;
            setStatus("Good morning!");
            platform::playSfx(platform::Sfx::LevelUp);
            return;
        }
    }
    // Home renovation: A on an END wall inside a Cottage/Hut/Manor pushes
    // that whole wall out one column - a serious construction project
    // (kCostExpand of raw materials per column).
    if (inInterior() && tile.placed == core::Placed::Wall) {
        core::InteriorData* room = roomContaining(tx, ty);
        core::Placed rk = room ? static_cast<core::Placed>(room->kind) : core::Placed::None;
        if (room && (rk == core::Placed::Cottage || rk == core::Placed::Hut ||
                     rk == core::Placed::Manor)) {
            int32_t ax, ay;
            interiorAnchor(room->bx, room->by, &ax, &ay);
            bool westEnd = tx == ax - room->wl;
            bool eastEnd = tx == ax + room->wr;
            if (westEnd || eastEnd) {
                if ((westEnd ? room->wl : room->wr) >= 14) {
                    setStatus("This wing can't stretch any further.");
                    platform::playSfx(platform::Sfx::Deny);
                    return;
                }
                if (!canAffordCost(core::kCostExpand)) {
                    setStatus("Expanding takes %d Wood, %d Stone", core::kCostExpand.wood,
                              core::kCostExpand.stone);
                    platform::playSfx(platform::Sfx::Deny);
                    return;
                }
                state_->inventory.remove(core::kItemWood, core::kCostExpand.wood);
                state_->inventory.remove(core::kItemStone, core::kCostExpand.stone);
                if (westEnd) room->wl++;
                else room->wr++;
                stampInterior(*room);
                awardXp(core::Skill::Building,
                        core::kXpBuildPerMaterial *
                            (core::kCostExpand.wood + core::kCostExpand.stone));
                actionTimer_ = 20;
                platform::playSfx(platform::Sfx::Place);
                setStatus("The %s wing grows!", westEnd ? "west" : "east");
                return;
            }
        }
        // Load-bearing ring walls can't be knocked out with A; fall
        // through so nothing else claims the press.
    }

    // Checking the mail: once a day the box has a little something in it
    // (a placed tile's timestamp field is otherwise unused).
    if (tile.placed == core::Placed::Mailbox) {
        if (core::elapsedAtLeast(tile.timestamp, now, core::kDayLengthSec)) {
            tile.timestamp = now;
            core::ItemId gift = (nextRand() % 2)
                                    ? core::kItemBerries
                                    : core::kSeedPoolT0[nextRand() % 4];
            state_->inventory.add(gift, 1);
            state_->world.markDirty(tx, ty);
            setStatus("You've got mail! +1 %s", core::kItemTable[gift].name);
        } else {
            setStatus("No mail today.");
        }
        platform::playSfx(platform::Sfx::Mail);
        return;
    }
    // Furniture restyling: A on a placed piece cycles its variant (stored
    // in the tile's decoTier) - chair facing, table wood, rug/bed/lamp
    // color. Chests are excluded (A opens them).
    if (tile.placed == core::Placed::Chair || tile.placed == core::Placed::Table ||
        tile.placed == core::Placed::Rug || tile.placed == core::Placed::RugLong ||
        tile.placed == core::Placed::Bed || tile.placed == core::Placed::Lamp ||
        tile.placed == core::Placed::Dresser || tile.placed == core::Placed::Stool ||
        tile.placed == core::Placed::Bench || tile.placed == core::Placed::Trough ||
        tile.placed == core::Placed::HayBale || tile.placed == core::Placed::Present ||
        (tile.placed == core::Placed::Wall && !inInterior())) {
        bool wood = tile.placed == core::Placed::Chair || tile.placed == core::Placed::Table ||
                    tile.placed == core::Placed::Dresser || tile.placed == core::Placed::Stool ||
                    tile.placed == core::Placed::Bench;
        int variants = wood ? 4 : 3;
        if (tile.placed == core::Placed::HayBale) variants = 2; // small/long bale
        if (tile.placed == core::Placed::Present) variants = 5; // wrap colors
        // Overworld walls toggle tall facade <-> low interior trim (the
        // 9-slice kit) - open-roof houses look great trimmed.
        if (tile.placed == core::Placed::Wall) variants = 2;
        if (tile.placed == core::Placed::Trough) {
            // Filling costs a Hay; emptying hands it back.
            if (tile.decoTier == 0) {
                if (!state_->inventory.remove(core::kItemHay, 1)) {
                    setStatus("Need 1 Hay");
                    platform::playSfx(platform::Sfx::Deny);
                    return;
                }
                tile.decoTier = 1;
                setStatus("Trough filled");
            } else {
                state_->inventory.add(core::kItemHay, 1);
                tile.decoTier = 0;
                setStatus("Hay taken back");
            }
            state_->world.markDirty(tx, ty);
            platform::playSfx(platform::Sfx::Chime);
            return;
        }
        tile.decoTier = static_cast<uint8_t>((tile.decoTier + 1) % variants);
        state_->world.markDirty(tx, ty);
        platform::playSfx(platform::Sfx::Chime);
        setStatus(tile.placed == core::Placed::Chair ? "Rotated" : "Restyled");
        return;
    }

    // 1.9 The clone on/next to the faced tile: cycle its orders.
    if (state_->clone.exists) {
        float cdx = state_->clone.pos.x - (static_cast<float>(tx) + 0.5f);
        float cdy = state_->clone.pos.y - (static_cast<float>(ty) + 0.5f);
        if (cdx * cdx + cdy * cdy < 1.4f) {
            state_->clone.task = static_cast<uint8_t>((state_->clone.task + 1) % 5);
            cloneHasTarget_ = false;
            static const char* kTaskNames[5] = {"Resting", "Lumberjack", "Miner", "Forager",
                                                "Farmer"};
            setStatus("Clone: %s", kTaskNames[state_->clone.task]);
            platform::playSfx(platform::Sfx::Chime);
            return;
        }
    }

    // 2. A wild animal on/next to the faced tile: try taming. Ambient
    // fish don't count - they'd swallow the cast when facing water.
    for (size_t i = 0; i < wild_.size(); i++) {
        if (wild_[i].kind == 3) continue;
        float dx = wild_[i].x - (static_cast<float>(tx) + 0.5f);
        float dy = wild_[i].y - (static_cast<float>(ty) + 0.5f);
        if (dx * dx + dy * dy < 1.1f) {
            if (tryTame(wild_[i], now)) {
                wild_.erase(wild_.begin() + static_cast<long>(i));
            }
            return;
        }
    }

    // 3. Ripe crop: harvest with anything.
    if (core::canHarvest(tile, now)) {
        const core::CropSpecies& species = core::kCropSpeciesTable[tile.cropSpeciesId];
        core::ItemId harvested = core::harvestCrop(tile, now);
        int amount = 1 + core::yieldBonus(state_->skillLevel(core::Skill::Farming));
        state_->inventory.add(harvested, amount);
        awardXp(core::Skill::Farming, species.harvestXp);
        setStatus("+%d %s", amount, core::kItemTable[harvested].name);
        state_->world.markDirty(tx, ty);
        actionTimer_ = 20;
        platform::playSfx(platform::Sfx::Harvest);
        return;
    }

    // 3.5 Loose pebble: pocket it, no tool needed.
    if (tile.decoration == core::Decoration::Pebble) {
        state_->inventory.add(core::kItemStone, 1);
        tile.decoration = core::Decoration::None;
        state_->world.markDirty(tx, ty);
        awardXp(core::Skill::Mining, 2);
        setStatus("+1 Stone");
        platform::playSfx(platform::Sfx::Dig);
        actionTimer_ = 20;
        return;
    }

    // 4. Forage nodes need no tool. Exception: an Axe on a picked-over
    // bush (waiting on its regrow clock) clears the stump for good instead
    // of the usual toolless forage - the opposite tool from what "grows"
    // it back, same idea as the Pickaxe on a tree stump below.
    if (tile.decoration == core::Decoration::Bush) {
        if (selected.item == core::kItemAxe && tile.depleted && !core::nodeReady(tile, now)) {
            tile.decoration = core::Decoration::None;
            tile.depleted = false;
            state_->world.markDirty(tx, ty);
            setStatus("Bush cleared");
            platform::playSfx(platform::Sfx::Demolish);
            actionTimer_ = 20;
            return;
        }
        gatherAt(tile, tx, ty, core::Decoration::Bush, core::kBushBalance, now);
        return;
    }
    if (tile.decoration == core::Decoration::Mushroom) {
        gatherAt(tile, tx, ty, core::Decoration::Mushroom, core::kMushroomBalance, now);
        return;
    }
    if (tile.decoration == core::Decoration::WildPumpkin ||
        tile.decoration == core::Decoration::WildSunflower) {
        if (wildStageAt(tile, tx, ty, now) < 3) {
            setStatus("Still growing...");
            return;
        }
        gatherAt(tile, tx, ty, tile.decoration, core::kWildPatchBalance, now);
        return;
    }

    // 4.5 Wild blooms: pick flowers for berries (and sometimes a seed).
    {
        int bloom = bloomSpriteAt(tile, tx, ty, now);
        if (bloom >= 0) {
            state_->inventory.add(core::kItemBerries, 1);
            char extra[40] = {0};
            if (rollPct(core::kBloomSeedChancePct)) {
                core::ItemId seed =
                    core::kSeedPoolT0[nextRand() % static_cast<uint32_t>(core::kSeedPoolSizes[0])];
                state_->inventory.add(seed, 1);
                snprintf(extra, sizeof(extra), " +1 %s", core::kItemTable[seed].name);
            }
            tile.timestamp = now;
            state_->world.markDirty(tx, ty);
            awardXp(core::Skill::Foraging, core::kXpBloom);
            setStatus("Wildflowers! +1 Berries%s", extra);
            platform::playSfx(platform::Sfx::Harvest);
            actionTimer_ = 20;
            return;
        }
    }

    // 5. Hammer equipped. On a Tree or Rock it does its OWN thing instead
    // of building - a gentle bonk that never depletes the node (unlike
    // the real tools) and never competes with Axe/Pickaxe for the actual
    // resource, just a small chance of a freebie. Anywhere else, it places
    // the loaded ghost building.
    if (selected.item == core::kItemHammer) {
        if (tile.decoration == core::Decoration::Tree && core::nodeReady(tile, now)) {
            actionTimer_ = 14;
            platform::playSfx(platform::Sfx::Chop);
            if (rollPct(20)) {
                int tier = clampTier(tile.decoTier);
                if (tier == 2) {
                    static const core::ItemId kFruit[4] = {core::kItemApple, core::kItemOrange,
                                                           core::kItemPear, core::kItemPeach};
                    core::ItemId fruit = kFruit[visHash(tx, ty) % 4];
                    state_->inventory.add(fruit, 1);
                    setStatus("You shake the tree - a %s falls!", core::kItemTable[fruit].name);
                } else {
                    state_->inventory.add(core::kItemSapling, 1);
                    setStatus("You shake the tree - a sapling falls!");
                }
            } else {
                setStatus("You shake the tree. Nothing falls.");
            }
            return;
        }
        if (tile.decoration == core::Decoration::Rock) {
            actionTimer_ = 14;
            platform::playSfx(platform::Sfx::Bonk);
            if (rollPct(15)) {
                state_->inventory.add(core::kItemStone, 1);
                setStatus("Bonk! A chip of stone breaks off.");
            } else {
                setStatus("Solid rock. A Pickaxe would do better.");
            }
            return;
        }
        core::ItemId ghost = kBuildables[buildGhostIdx_].item;
        const core::BuildCost& cost = kBuildables[buildGhostIdx_].cost;
        if (buildFromMaterials(tile, tx, ty, ghost, cost)) {
            actionTimer_ = 20;
        } else if (!canAffordCost(cost)) {
            if (cost.stone > 0) {
                setStatus("Need %d Wood, %d Stone", cost.wood, cost.stone);
            } else {
                setStatus("Need %d Wood", cost.wood);
            }
        } else {
            setStatus("Can't place there");
        }
        return;
    }

    // 5.5 Toolless fruit picking: a ready fruit tree (tier 2) lets you take
    // one piece of fruit with anything but an Axe equipped (which commits
    // to felling it instead) - no tool needed, same spirit as foraging a
    // bush. Cooldown re-uses the tile's timestamp (meaningless while the
    // tree isn't depleted) so it can't be spammed every frame.
    if (tile.decoration == core::Decoration::Tree && clampTier(tile.decoTier) == 2 &&
        core::nodeReady(tile, now) && selected.item != core::kItemAxe) {
        if (core::elapsedAtLeast(tile.timestamp, now, core::kFruitPickCooldownSec)) {
            static const core::ItemId kFruit[4] = {core::kItemApple, core::kItemOrange,
                                                    core::kItemPear, core::kItemPeach};
            core::ItemId fruit = kFruit[visHash(tx, ty) % 4];
            state_->inventory.add(fruit, 1);
            tile.timestamp = now;
            state_->world.markDirty(tx, ty);
            awardXp(core::Skill::Foraging, core::kXpFruitPick);
            setStatus("Picked a ripe %s!", core::kItemTable[fruit].name);
            platform::playSfx(platform::Sfx::Harvest);
            actionTimer_ = 20;
        } else {
            setStatus("Already picked recently - give it time.");
        }
        return;
    }

    // 6. The Hoe works the soil (a real tool action, with the real hoe
    // swing to match). Digging a hole stays toolless on B (doDigAction).
    if (selected.item == core::kItemHoe && core::canTill(tile)) {
        bool wasGrass = tile.terrain == core::Terrain::Grass;
        core::tillTile(tile);
        state_->world.markDirty(tx, ty);
        awardXp(core::Skill::Farming, core::kXpTill);
        actionTimer_ = 20;
        platform::playSfx(platform::Sfx::Till);
        if (wasGrass && rollPct(core::kTillHayChancePct)) {
            state_->inventory.add(core::kItemHay, 1);
            setStatus("Tilled +1 Hay");
        }
        return;
    }

    // 7. Tool / item on the faced tile.
    switch (selected.item) {
        case core::kItemAxe:
            if (tile.decoration == core::Decoration::Tree) {
                gatherAt(tile, tx, ty, core::Decoration::Tree, core::kTreeBalance, now);
            } else if (tile.decoration == core::Decoration::Rock) {
                setStatus("An Axe won't scratch that - try a Pickaxe.");
                platform::playSfx(platform::Sfx::Deny);
            }
            return;

        case core::kItemPickaxe:
            if (tile.decoration == core::Decoration::Rock) {
                gatherAt(tile, tx, ty, core::Decoration::Rock, core::kRockBalance, now);
            } else if (tile.decoration == core::Decoration::Tree) {
                // A stump waiting on its regrow clock: the Pickaxe grubs it
                // out for good (opposite tool from the Axe that felled it).
                // A still-standing tree ignores the Pickaxe as before.
                if (tile.depleted && !core::nodeReady(tile, now)) {
                    tile.decoration = core::Decoration::None;
                    tile.depleted = false;
                    state_->world.markDirty(tx, ty);
                    setStatus("Stump cleared");
                    platform::playSfx(platform::Sfx::Demolish);
                    actionTimer_ = 20;
                } else {
                    setStatus("A Pickaxe won't cut that - try an Axe.");
                    platform::playSfx(platform::Sfx::Deny);
                }
            }
            return;

        case core::kItemWateringCan:
            // Fill from open water, or from a placed Well (the point of
            // building one at home).
            if (tile.terrain == core::Terrain::Water || tile.placed == core::Placed::Well) {
                state_->toolBelt.add(core::kItemWateringCanFull);
                selectItem(core::kItemWateringCanFull);
                setStatus("Watering can filled");
                actionTimer_ = 20;
                platform::playSfx(platform::Sfx::Plant);
            }
            return;

        case core::kItemWateringCanFull:
            // Watering a crop makes it grow 1.5x faster (one watering per
            // planting); pouring into a hole still makes a pond. A placed
            // Water Tray fills right up (the animals appreciate it).
            if (tile.placed == core::Placed::WaterTray && tile.decoTier != 0) {
                tile.decoTier = 0;
                state_->world.markDirty(tx, ty);
                state_->toolBelt.add(core::kItemWateringCan);
                selectItem(core::kItemWateringCan);
                setStatus("Tray filled!");
                actionTimer_ = 20;
                platform::playSfx(platform::Sfx::Plant);
            } else if (tile.hasCrop && !tile.watered) {
                tile.watered = true;
                state_->world.markDirty(tx, ty);
                state_->toolBelt.add(core::kItemWateringCan);
                selectItem(core::kItemWateringCan);
                setStatus("Watered - it'll grow faster!");
                actionTimer_ = 20;
                platform::playSfx(platform::Sfx::Plant);
            } else if (core::pourWater(tile)) {
                state_->world.markDirty(tx, ty);
                state_->toolBelt.add(core::kItemWateringCan);
                selectItem(core::kItemWateringCan);
                // Water finds its level - connected dug holes flood too.
                int spread = 0;
                static const int kD[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
                for (const auto& d : kD) spread += floodWaterFrom(tx + d[0], ty + d[1]);
                setStatus(spread > 0 ? "Poured - water spreads!" : "Poured - water!");
                actionTimer_ = 20;
                platform::playSfx(platform::Sfx::Plant);
            }
            return;

        case core::kItemFishingRod:
            if (tile.terrain == core::Terrain::Water) {
                fishState_ = 1;
                fishTimer_ = core::kBiteMinSec +
                             static_cast<float>(nextRand() % 100) / 100.0f *
                                 (core::kBiteMaxSec - core::kBiteMinSec);
                // Fish love rain.
                if (core::weatherAt(state_->worldSeed, now) == core::Weather::Rain) {
                    fishTimer_ *= core::kRainBiteMul;
                }
                setStatus("Cast... (A when it bites!)");
                platform::playSfx(platform::Sfx::Plant);
            }
            return;

        case core::kItemWood:
            // A pile of wood in hand builds the Workbench directly - the
            // bootstrap build that needs no Hammer (which is crafted AT
            // the bench). The ghost preview shows where it lands.
            if (buildFromMaterials(tile, tx, ty, core::kItemWorkbench, core::kCostWorkbench)) {
                actionTimer_ = 20;
                setStatus("Workbench built! Press A on it to craft tools");
            } else if (!canAffordCost(core::kCostWorkbench)) {
                setStatus("A Workbench takes %d Wood", core::kCostWorkbench.wood);
            } else {
                setStatus("Can't build here");
            }
            return;

        case core::kItemSapling:
            if (core::plantSapling(tile, now)) {
                state_->inventory.remove(core::kItemSapling, 1);
                state_->world.markDirty(tx, ty);
                setStatus("Sapling planted");
                actionTimer_ = 20;
                platform::playSfx(platform::Sfx::Plant);
            }
            return;

        default:
            break;
    }

    // 8. Seeds.
    for (int i = 0; i < core::kCropSpeciesCount; i++) {
        if (selected.item == core::kCropSpeciesTable[i].seedItem && core::canPlant(tile)) {
            if (state_->inventory.remove(selected.item, 1)) {
                core::plantCrop(tile, static_cast<uint8_t>(i), now);
                state_->world.markDirty(tx, ty);
                awardXp(core::Skill::Farming, core::kXpPlant);
                actionTimer_ = 20;
                platform::playSfx(platform::Sfx::Plant);
            }
            return;
        }
    }

    // 9. Placeables selected directly (without going through the Hammer).
    if (core::kItemTable[selected.item].category == core::ItemCategory::Placeable) {
        if (placeSelected(tile, tx, ty, selected.item)) return;
    }

    // 10. Nothing else to do and standing on a rail: hop in a minecart.
    int32_t px = static_cast<int32_t>(std::floor(state_->playerPos.x));
    int32_t py = static_cast<int32_t>(std::floor(state_->playerPos.y));
    if (state_->world.tileAt(px, py).terrain == core::Terrain::Rail) {
        auto railTo = [&](int dx, int dy) {
            return state_->world.tileAt(px + dx, py + dy).terrain == core::Terrain::Rail;
        };
        // Set off the way you're facing if track continues there,
        // otherwise the first connected direction.
        core::Vec2f f = facingOffset();
        int fdx = static_cast<int>(f.x), fdy = static_cast<int>(f.y);
        int dx = 0, dy = 0;
        if (railTo(fdx, fdy)) {
            dx = fdx;
            dy = fdy;
        } else {
            static const int kDirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
            for (const auto& d : kDirs) {
                if (railTo(d[0], d[1])) {
                    dx = d[0];
                    dy = d[1];
                    break;
                }
            }
        }
        if (dx != 0 || dy != 0) {
            riding_ = true;
            rideDirX_ = dx;
            rideDirY_ = dy;
            rideDecidedX_ = px;
            rideDecidedY_ = py;
            state_->playerPos = {static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f};
            state_->facing = dx > 0 ? core::Facing::Right
                             : dx < 0 ? core::Facing::Left
                             : dy < 0 ? core::Facing::Up
                                      : core::Facing::Down;
            setStatus("Wheee! (A or B to hop out)");
            platform::playSfx(platform::Sfx::Ui);
        } else {
            setStatus("This track goes nowhere...");
        }
    }
}

// Water finds its level: BFS from (sx,sy) through connected dug Holes,
// converting them to Water. Bounded so a mega-trench can't hitch a frame.
int WorldScene::floodWaterFrom(int32_t sx, int32_t sy) {
    struct P {
        int32_t x, y;
    };
    P queue[128];
    int head = 0, tail = 0, converted = 0;
    queue[tail++] = {sx, sy};
    while (head < tail) {
        P pt = queue[head++];
        core::Tile& t = state_->world.tileAt(pt.x, pt.y);
        if (t.terrain != core::Terrain::Hole || t.placed != core::Placed::None) continue;
        t.terrain = core::Terrain::Water;
        state_->world.markDirty(pt.x, pt.y);
        converted++;
        static const int kD[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        for (const auto& d : kD) {
            if (tail < 128) queue[tail++] = {pt.x + d[0], pt.y + d[1]};
        }
    }
    return converted;
}

void WorldScene::doDigAction() {
    core::Vec2f offset = facingOffset();
    int32_t tx = static_cast<int32_t>(std::floor(state_->playerPos.x + offset.x));
    int32_t ty = static_cast<int32_t>(std::floor(state_->playerPos.y + offset.y));
    core::Tile& tile = state_->world.tileAt(tx, ty);

    if (tile.placed == core::Placed::Chest) {
        core::ChestData* chest = state_->chestAt(tx, ty);
        bool empty = true;
        if (chest) {
            for (int i = 0; i < core::Inventory::slotCount(); i++) {
                if (chest->items.slot(i).item != core::kItemNone) {
                    empty = false;
                    break;
                }
            }
        }
        if (!empty) {
            setStatus("Chest isn't empty!");
            return;
        }
    }
    if (tile.placed == core::Placed::Coop || tile.placed == core::Placed::Barn) {
        for (const core::TamedAnimal& a : state_->animals) {
            if (a.homeX == tx && a.homeY == ty) {
                setStatus("Animals live here!");
                return;
            }
        }
    }

    // Interior ring walls (and the exit door) are load-bearing - the
    // shovel can't demolish the room itself, only what the player added.
    if (inInterior() &&
        (tile.placed == core::Placed::Wall || tile.placed == core::Placed::Door)) {
        if (core::InteriorData* room = roomContaining(tx, ty)) {
            int32_t ax, ay;
            interiorAnchor(room->bx, room->by, &ax, &ay);
            if (tx == ax - room->wl || tx == ax + room->wr || ty == ay ||
                ty == ay + room->h - 1) {
                setStatus("That wall holds the building up!");
                platform::playSfx(platform::Sfx::Deny);
                return;
            }
        }
    }

    core::Placed wasPlaced = tile.placed;
    core::ShovelResult result = core::useShovel(tile);
    if (result == core::ShovelResult::Blocked) return;
    state_->world.markDirty(tx, ty);
    actionTimer_ = 20;
    // Freshly bared dirt (un-tilled bed, dug-up path, filled hole) starts
    // its heal-back-to-grass clock (see the regrow sweep in update()).
    if (result == core::ShovelResult::Untilled || result == core::ShovelResult::Filled) {
        tile.timestamp = core::nowSeconds();
    }

    if (result == core::ShovelResult::Removed) {
        if (wasPlaced == core::Placed::Chest) {
            for (size_t i = 0; i < state_->chests.size(); i++) {
                if (state_->chests[i].x == tx && state_->chests[i].y == ty) {
                    state_->chests.erase(state_->chests.begin() + static_cast<long>(i));
                    break;
                }
            }
        }
        if (wasPlaced == core::Placed::Beehive) {
            for (size_t i = 0; i < state_->hives.size(); i++) {
                if (state_->hives[i].x == tx && state_->hives[i].y == ty) {
                    state_->hives.erase(state_->hives.begin() + static_cast<long>(i));
                    break;
                }
            }
        }
        if (state_->home.set && state_->home.x == tx && state_->home.y == ty) {
            state_->home.set = false; // demolished the camp
        }
        setStatus("Demolished");
        platform::playSfx(platform::Sfx::Demolish);
    } else if (result == core::ShovelResult::Dug) {
        platform::playSfx(platform::Sfx::Dig);
        if (rollPct(core::kDigStoneChancePct)) {
            state_->inventory.add(core::kItemStone, 1);
            setStatus("Dug +1 Stone");
        }
        // Dig beside water and it rushes in - and floods any connected
        // trench of holes. Canals, dug the honest way.
        static const int kD[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        for (const auto& d : kD) {
            if (state_->world.tileAt(tx + d[0], ty + d[1]).terrain == core::Terrain::Water) {
                int n = floodWaterFrom(tx, ty);
                if (n > 0) setStatus(n > 1 ? "Water rushes through!" : "Water rushes in!");
                break;
            }
        }
    } else {
        platform::playSfx(platform::Sfx::Dig);
    }
}

// --- Wild animals -----------------------------------------------------------

void WorldScene::updateWildAnimals(float dt) {
    spawnTimer_ -= dt;
    if (spawnTimer_ <= 0.0f) {
        spawnTimer_ = 4.0f;
        // No wildlife indoors (existing critters despawn by distance).
        if (wild_.size() < 6 && !inInterior()) {
            // Spawn 7-11 tiles out at a random angle, on a walkable tile.
            float angle = static_cast<float>(nextRand() % 628) / 100.0f;
            float dist = 7.0f + static_cast<float>(nextRand() % 40) / 10.0f;
            int32_t sx = static_cast<int32_t>(std::floor(state_->playerPos.x + std::cos(angle) * dist));
            int32_t sy = static_cast<int32_t>(std::floor(state_->playerPos.y + std::sin(angle) * dist));
            bool frozenSpawn = core::biomeAt(state_->worldSeed, sx, sy) == core::Biome::Snow;
            const core::Tile& spawnTile = state_->world.tileAt(sx, sy);
            if (spawnTile.terrain == core::Terrain::Water && !frozenSpawn &&
                spawnTile.placed == core::Placed::None) {
                // Open water spawns an ambient fish cruising the pond
                // (small or medium strip art; never tameable or catchable
                // - the rod's catches are their own pools).
                WildAnimal w;
                w.kind = 3;
                w.variant = static_cast<uint8_t>(nextRand() % 2);
                w.x = w.tx = static_cast<float>(sx) + 0.5f;
                w.y = w.ty = static_cast<float>(sy) + 0.5f;
                w.moveTimer = 1.0f + static_cast<float>(nextRand() % 20) / 10.0f;
                wild_.push_back(w);
            } else if (!core::blocksMovement(spawnTile, frozenSpawn)) {
                WildAnimal w;
                // Frogs only appear beside water; otherwise chicken-heavy.
                bool nearWater = false;
                for (int dy = -1; dy <= 1 && !nearWater; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (state_->world.tileAt(sx + dx, sy + dy).terrain == core::Terrain::Water) {
                            nearWater = true;
                            break;
                        }
                    }
                }
                uint32_t roll = nextRand() % 10;
                if (nearWater && roll >= 6) {
                    w.kind = 2; // frog
                } else {
                    w.kind = (roll < 3) ? 1 : 0; // 30% cow, else chicken
                }
                w.variant = static_cast<uint8_t>(nextRand() % 5);
                w.x = w.tx = static_cast<float>(sx) + 0.5f;
                w.y = w.ty = static_cast<float>(sy) + 0.5f;
                w.moveTimer = 1.0f + static_cast<float>(nextRand() % 20) / 10.0f;
                wild_.push_back(w);
            }
        }
    }

    for (size_t i = 0; i < wild_.size();) {
        WildAnimal& w = wild_[i];
        float pdx = w.x - state_->playerPos.x;
        float pdy = w.y - state_->playerPos.y;
        if (pdx * pdx + pdy * pdy > 18.0f * 18.0f) {
            wild_.erase(wild_.begin() + static_cast<long>(i));
            continue;
        }

        if (w.reqT > 0) w.reqT--;

        w.moveTimer -= dt;
        if (w.moveTimer <= 0.0f) {
            w.moveTimer = 2.0f + static_cast<float>(nextRand() % 20) / 10.0f;
            static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            const int* d = dirs[nextRand() % 4];
            int32_t nx = static_cast<int32_t>(std::floor(w.x)) + d[0];
            int32_t ny = static_cast<int32_t>(std::floor(w.y)) + d[1];
            bool frozenStep = core::biomeAt(state_->worldSeed, nx, ny) == core::Biome::Snow;
            const core::Tile& stepTile = state_->world.tileAt(nx, ny);
            bool stepOk = w.kind == 3
                              ? (stepTile.terrain == core::Terrain::Water && !frozenStep &&
                                 stepTile.placed == core::Placed::None)
                              : (!core::blocksMovement(stepTile, frozenStep,
                                                       core::nowSeconds()) &&
                                 !cliffBlocked(static_cast<int32_t>(std::floor(w.x)),
                                               static_cast<int32_t>(std::floor(w.y)), nx, ny));
            if (stepOk) {
                w.tx = static_cast<float>(nx) + 0.5f;
                w.ty = static_cast<float>(ny) + 0.5f;
            }
        }

        float mdx = w.tx - w.x;
        float mdy = w.ty - w.y;
        float len = std::sqrt(mdx * mdx + mdy * mdy);
        if (len > 0.02f) {
            float step = 1.5f * dt;
            if (step > len) step = len;
            w.x += mdx / len * step;
            w.y += mdy / len * step;
            if (mdx < -0.02f) w.faceLeft = true;
            else if (mdx > 0.02f) w.faceLeft = false;
        }
        i++;
    }
}

// --- The Clone Mirror ---------------------------------------------------------

void WorldScene::updateClone(float dt) {
    core::CloneData& cl = state_->clone;
    if (!cl.exists) return;
    if (cloneCd_ > 0.0f) cloneCd_ -= dt;
    cloneMoving_ = false;
    if (cl.task == 0) return; // resting

    int64_t now = core::nowSeconds();
    int32_t ccx = static_cast<int32_t>(std::floor(cl.pos.x));
    int32_t ccy = static_cast<int32_t>(std::floor(cl.pos.y));

    // Periodically scan for the nearest job within the work radius.
    cloneScanT_ -= dt;
    if (!cloneHasTarget_ && cloneScanT_ <= 0.0f) {
        cloneScanT_ = 1.0f;
        int64_t bestD = INT64_MAX;
        constexpr int R = core::kCloneWorkRadius;
        for (int32_t sy = ccy - R; sy <= ccy + R; sy++) {
            for (int32_t sx = ccx - R; sx <= ccx + R; sx++) {
                const core::Tile& t = state_->world.tileAt(sx, sy);
                bool want = false;
                int tier = clampTier(t.decoTier);
                switch (cl.task) {
                    case 1: // lumberjack
                        want = t.decoration == core::Decoration::Tree &&
                               core::nodeReady(t, now) &&
                               state_->skillLevel(core::Skill::Logging) >=
                                   core::kTreeBalance.levelReq[tier];
                        break;
                    case 2: // miner (rocks + loose pebbles)
                        want = (t.decoration == core::Decoration::Rock &&
                                core::nodeReady(t, now) &&
                                state_->skillLevel(core::Skill::Mining) >=
                                    core::kRockBalance.levelReq[tier]) ||
                               t.decoration == core::Decoration::Pebble;
                        break;
                    case 3: // forager (bushes, mushrooms, RIPE wild patches)
                        want = (t.decoration == core::Decoration::Bush ||
                                t.decoration == core::Decoration::Mushroom ||
                                ((t.decoration == core::Decoration::WildPumpkin ||
                                  t.decoration == core::Decoration::WildSunflower) &&
                                 wildStageAt(t, sx, sy, now) == 3)) &&
                               core::nodeReady(t, now) &&
                               state_->skillLevel(core::Skill::Foraging) >=
                                   (t.decoration == core::Decoration::Bush
                                        ? core::kBushBalance.levelReq[tier]
                                    : t.decoration == core::Decoration::Mushroom
                                        ? core::kMushroomBalance.levelReq[tier]
                                        : core::kWildPatchBalance.levelReq[tier]);
                        break;
                    case 4: // farmer: harvest > water > plant
                        want = (t.hasCrop &&
                                (core::canHarvest(t, now) || !t.watered)) ||
                               (t.tilled && !t.hasCrop && t.placed == core::Placed::None);
                        break;
                    default: break;
                }
                if (!want) continue;
                int64_t d = static_cast<int64_t>(sx - ccx) * (sx - ccx) +
                            static_cast<int64_t>(sy - ccy) * (sy - ccy);
                if (d < bestD) {
                    bestD = d;
                    cloneTX_ = sx;
                    cloneTY_ = sy;
                    cloneHasTarget_ = true;
                }
            }
        }
    }
    if (!cloneHasTarget_) return;

    // Close enough: act (on the work cooldown). Otherwise walk toward it.
    float tdx = static_cast<float>(cloneTX_) + 0.5f - cl.pos.x;
    float tdy = static_cast<float>(cloneTY_) + 0.5f - cl.pos.y;
    float dist2 = tdx * tdx + tdy * tdy;
    if (dist2 < 1.3f) {
        cloneStuckT_ = 0.0f;
        if (cloneCd_ <= 0.0f) {
            cloneCd_ = core::kCloneWorkSec;
            cloneAct(cloneTX_, cloneTY_);
            cloneHasTarget_ = false;
        }
        return;
    }

    // Greedy per-axis walk (same rules as everyone else: no water, no
    // cliff-hopping). If blocked too long, give up on this target.
    float len = std::sqrt(dist2);
    float step = 2.0f * dt;
    bool frozen = core::biomeAt(state_->worldSeed, ccx, ccy) == core::Biome::Snow;
    auto tryStep = [&](float dx, float dy) {
        float nx = cl.pos.x + dx, ny = cl.pos.y + dy;
        int32_t txi = static_cast<int32_t>(std::floor(nx));
        int32_t tyi = static_cast<int32_t>(std::floor(ny));
        bool sameTile = txi == ccx && tyi == ccy;
        if (!sameTile &&
            core::blocksMovement(state_->world.tileAt(txi, tyi), frozen, now)) {
            return false;
        }
        if (cliffBlocked(ccx, ccy, txi, tyi)) return false;
        cl.pos.x = nx;
        cl.pos.y = ny;
        return true;
    };
    bool movedX = std::fabs(tdx) > 0.05f && tryStep(tdx / len * step, 0.0f);
    bool movedY = std::fabs(tdy) > 0.05f && tryStep(0.0f, tdy / len * step);
    cloneMoving_ = movedX || movedY;
    if (cloneMoving_) {
        cloneStuckT_ = 0.0f;
        if (std::fabs(tdx) > std::fabs(tdy)) {
            cloneDir_ = tdx > 0 ? 3 : 2; // right / left
        } else {
            cloneDir_ = tdy > 0 ? 0 : 1; // down / up
        }
    } else {
        cloneStuckT_ += dt;
        if (cloneStuckT_ > 3.0f) {
            cloneStuckT_ = 0.0f;
            cloneHasTarget_ = false; // path's no good - pick another job
        }
    }
}

void WorldScene::cloneAct(int32_t tx, int32_t ty) {
    core::Tile& t = state_->world.tileAt(tx, ty);
    int64_t now = core::nowSeconds();
    core::CloneData& cl = state_->clone;
    int tier = clampTier(t.decoTier);
    bool nearPlayer =
        std::fabs(cl.pos.x - state_->playerPos.x) + std::fabs(cl.pos.y - state_->playerPos.y) <
        12.0f;

    switch (cl.task) {
        case 1: // lumberjack
            if (core::gatherNode(t, core::Decoration::Tree,
                                 state_->skillLevel(core::Skill::Logging), now) ==
                core::GatherResult::Ok) {
                cloneDeposit(core::kItemWood, core::kTreeBalance.baseYield[tier]);
                poofs_.push_back({tx, ty, 0.0f});
                state_->world.markDirty(tx, ty);
                if (nearPlayer) platform::playSfx(platform::Sfx::Chop);
            }
            break;
        case 2: // miner (rocks vanish when mined; pebbles picked up)
            if (t.decoration == core::Decoration::Pebble) {
                cloneDeposit(core::kItemStone, 1);
                t.decoration = core::Decoration::None;
                state_->world.markDirty(tx, ty);
                break;
            }
            if (core::gatherNode(t, core::Decoration::Rock,
                                 state_->skillLevel(core::Skill::Mining), now) ==
                core::GatherResult::Ok) {
                cloneDeposit(core::kItemStone, core::kRockBalance.baseYield[tier]);
                if (rollPct(core::kOreChancePct[tier])) cloneDeposit(core::kItemOre, 1);
                t.decoration = core::Decoration::None;
                t.depleted = false;
                state_->world.markDirty(tx, ty);
                if (nearPlayer) platform::playSfx(platform::Sfx::Mine);
            }
            break;
        case 3: { // forager
            core::Decoration kind = t.decoration;
            core::ItemId got;
            const core::NodeBalance* bal;
            switch (kind) {
                case core::Decoration::Bush:
                    got = core::kItemBerries;
                    bal = &core::kBushBalance;
                    break;
                case core::Decoration::Mushroom:
                    got = core::kItemMushroom;
                    bal = &core::kMushroomBalance;
                    break;
                case core::Decoration::WildPumpkin:
                    got = core::kItemPumpkin;
                    bal = &core::kWildPatchBalance;
                    break;
                case core::Decoration::WildSunflower:
                    got = core::kItemSunflower;
                    bal = &core::kWildPatchBalance;
                    break;
                default:
                    return;
            }
            if (core::gatherNode(t, kind, state_->skillLevel(core::Skill::Foraging), now) ==
                core::GatherResult::Ok) {
                cloneDeposit(got, bal->baseYield[tier]);
                state_->world.markDirty(tx, ty);
                if (nearPlayer) platform::playSfx(platform::Sfx::Harvest);
            }
            break;
        }
        case 4: // farmer: harvest > water > plant-from-chest
            if (t.hasCrop && core::canHarvest(t, now)) {
                core::ItemId got = core::harvestCrop(t, now);
                cloneDeposit(got, 1);
                state_->world.markDirty(tx, ty);
                if (nearPlayer) platform::playSfx(platform::Sfx::Harvest);
            } else if (t.hasCrop && !t.watered) {
                t.watered = true;
                state_->world.markDirty(tx, ty);
                if (nearPlayer) platform::playSfx(platform::Sfx::Plant);
            } else if (t.tilled && !t.hasCrop && t.placed == core::Placed::None) {
                int species = cloneTakeSeed();
                if (species >= 0) {
                    core::plantCrop(t, static_cast<uint8_t>(species), now);
                    state_->world.markDirty(tx, ty);
                    if (nearPlayer) platform::playSfx(platform::Sfx::Plant);
                }
            }
            break;
        default:
            break;
    }
}

void WorldScene::cloneDeposit(core::ItemId item, int count) {
    if (count <= 0) return;
    int32_t ccx = static_cast<int32_t>(std::floor(state_->clone.pos.x));
    int32_t ccy = static_cast<int32_t>(std::floor(state_->clone.pos.y));
    // Nearest chest with room wins.
    core::ChestData* best = nullptr;
    int64_t bestD = INT64_MAX;
    for (core::ChestData& c : state_->chests) {
        int64_t d = static_cast<int64_t>(c.x - ccx) * (c.x - ccx) +
                    static_cast<int64_t>(c.y - ccy) * (c.y - ccy);
        if (d <= static_cast<int64_t>(core::kCloneChestRadius) * core::kCloneChestRadius &&
            d < bestD) {
            bestD = d;
            best = &c;
        }
    }
    int left = count;
    if (best) left -= best->items.add(item, left);
    if (left > 0) {
        // No chest (or full): leave it on the ground at the clone's feet.
        if (core::GroundItem* existing = state_->groundItemAt(ccx, ccy)) {
            if (existing->item == item) {
                existing->count = static_cast<uint16_t>(existing->count + left);
            }
        } else {
            core::GroundItem drop;
            drop.x = ccx;
            drop.y = ccy;
            drop.item = item;
            drop.count = static_cast<uint16_t>(left);
            state_->groundItems.push_back(drop);
        }
    }
}

int WorldScene::cloneTakeSeed() {
    int32_t ccx = static_cast<int32_t>(std::floor(state_->clone.pos.x));
    int32_t ccy = static_cast<int32_t>(std::floor(state_->clone.pos.y));
    for (core::ChestData& c : state_->chests) {
        int64_t d = static_cast<int64_t>(c.x - ccx) * (c.x - ccx) +
                    static_cast<int64_t>(c.y - ccy) * (c.y - ccy);
        if (d > static_cast<int64_t>(core::kCloneChestRadius) * core::kCloneChestRadius) continue;
        for (int i = 0; i < core::kCropSpeciesCount; i++) {
            core::ItemId seed = core::kCropSpeciesTable[i].seedItem;
            if (c.items.countOf(seed) > 0 && c.items.remove(seed, 1)) return i;
        }
    }
    return -1;
}

// --- The Mine (Milestone 3) --------------------------------------------------

namespace {

// Ore node types, in depth order. Index = mine_ cell kind minus 10.
struct NodeInfo {
    int minFloor;
    int weight;
    core::ItemId drop;
    int dropCount;
    core::ItemId bonus; // kItemNone = no bonus drop
    int xp;
};
constexpr NodeInfo kNodes[8] = {
    /* stone    */ {1, 30, core::kItemStone, 2, core::kItemNone, 10},
    /* coal     */ {1, 25, core::kItemCoal, 2, core::kItemStone, 12},
    /* copper   */ {3, 18, core::kItemOre, 1, core::kItemStone, 16},
    /* gold     */ {5, 10, core::kItemOre, 2, core::kItemNone, 20},
    /* ruby     */ {5, 8, core::kItemRuby, 1, core::kItemNone, 25},
    /* diamond  */ {7, 6, core::kItemDiamond, 1, core::kItemNone, 30},
    /* emerald  */ {7, 6, core::kItemEmerald, 1, core::kItemNone, 30},
    /* amethyst */ {9, 4, core::kItemAmethyst, 1, core::kItemNone, 35},
};

} // namespace

void WorldScene::enterMine() {
    // The overworld position stays parked beside the shaft, so saving or
    // exiting from any depth is always safe.
    mineFloor_ = 1;
    hp_ = core::kMaxHp;
    invulnT_ = 0.0f;
    atkT_ = 0.0f;
    fishState_ = 0;
    riding_ = false;
    generateMineFloor();
    setStatus("The mine... it's dark down here.");
    platform::playSfx(platform::Sfx::Descend);
}

void WorldScene::exitMine(const char* msg) {
    mineFloor_ = 0;
    riding_ = false;
    foes_.clear();
    setStatus("%s", msg);
    platform::playSfx(platform::Sfx::Ui);
}

void WorldScene::generateMineFloor() {
    // Solid rock, then drunken miners carve it out: snaking tunnels,
    // blasted-out chambers, branches and dead ends - a different cave
    // shape every floor, and connected by construction since every cut
    // starts from carved ground. Deeper floors are tighter and twistier.
    for (int y = 0; y < kMineH; y++) {
        for (int x = 0; x < kMineW; x++) mine_[y][x] = 1;
    }
    int pct = 52 - mineFloor_ * 2;
    if (pct < 34) pct = 34;
    int target = (kMineW - 2) * (kMineH - 2) * pct / 100;
    // The miner works through shuffled waypoints spanning the whole map,
    // half-biased toward the current one - so tunnels reach every region
    // and no floor ends up as one blob hugging an edge.
    int wpx[4] = {kMineW - 4, kMineW - 4, 4, kMineW / 2};
    int wpy[4] = {kMineH - 4, 3, kMineH - 4, kMineH / 2};
    if (nextRand() % 2) {
        std::swap(wpx[0], wpx[1]);
        std::swap(wpy[0], wpy[1]);
    }
    if (nextRand() % 2) {
        std::swap(wpx[2], wpx[3]);
        std::swap(wpy[2], wpy[3]);
    }
    int wp = 0;
    int cx = 3, cy = 3, open = 0, guard = 0;
    while (open < target && guard++ < 20000) {
        if (mine_[cy][cx] == 1) {
            mine_[cy][cx] = 0;
            open++;
        }
        // The odd blast opens a small chamber around the miner.
        if (nextRand() % 41 == 0) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx >= 1 && ny >= 1 && nx <= kMineW - 2 && ny <= kMineH - 2 &&
                        mine_[ny][nx] == 1) {
                        mine_[ny][nx] = 0;
                        open++;
                    }
                }
            }
        }
        // Occasionally restart from somewhere already carved - branches.
        if (nextRand() % 67 == 0) {
            for (int t = 0; t < 40; t++) {
                int rx = 1 + static_cast<int>(nextRand() % (kMineW - 2));
                int ry = 1 + static_cast<int>(nextRand() % (kMineH - 2));
                if (mine_[ry][rx] == 0) {
                    cx = rx;
                    cy = ry;
                    break;
                }
            }
        }
        int tx2 = wpx[wp], ty2 = wpy[wp];
        if (cx == tx2 && cy == ty2 && wp < 3) wp++;
        if (nextRand() % 2 == 0) {
            // Biased step toward the current waypoint.
            if (tx2 != cx && (ty2 == cy || nextRand() % 2)) {
                cx += tx2 > cx ? 1 : -1;
            } else if (ty2 != cy) {
                cy += ty2 > cy ? 1 : -1;
            }
        } else {
            int d = static_cast<int>(nextRand() % 4);
            cx += (d == 0) - (d == 1);
            cy += (d == 2) - (d == 3);
        }
        if (cx < 1) cx = 1;
        if (cx > kMineW - 2) cx = kMineW - 2;
        if (cy < 1) cy = 1;
        if (cy > kMineH - 2) cy = kMineH - 2;
    }
    // Spawn corner: exit portal at (2,2), player just beside it.
    for (int y = 1; y <= 4; y++) {
        for (int x = 1; x <= 4; x++) mine_[y][x] = 0;
    }
    mine_[2][2] = 3;
    minePos_ = {3.5f, 3.5f};
    pKnockX_ = pKnockY_ = 0.0f;

    // Hole down in the far half; if the carve never reached out there,
    // cut a corridor west from it until it meets open ground.
    bool holeDown = false;
    for (int tries = 0; tries < 200 && !holeDown; tries++) {
        int x = kMineW / 2 + static_cast<int>(nextRand() % (kMineW / 2 - 2));
        int y = 2 + static_cast<int>(nextRand() % (kMineH - 4));
        if (mine_[y][x] == 0) {
            mine_[y][x] = 2;
            holeDown = true;
        }
    }
    if (!holeDown) {
        int y = kMineH / 2, x = kMineW - 3;
        mine_[y][x] = 2;
        for (int cxw = x - 1; cxw >= 1 && mine_[y][cxw] == 1; cxw--) mine_[y][cxw] = 0;
    }
    // Some floors hold a built BRICK ROOM (the old delvings): rectangular
    // grey-stone walls with a walkable arch doorway in the south face,
    // interior carved open - ores and foes may move in, making them
    // little treasure vaults.
    if (nextRand() % 100 < 55) {
        int rw = 6 + static_cast<int>(nextRand() % 4);
        // North AND south walls are TWO rows thick - wall-top band over
        // the brick face outside, the top band visible from inside -
        // exactly how the pack's promo assembles its rooms.
        int rh = 6 + static_cast<int>(nextRand() % 3);
        int rx = 6 + static_cast<int>(nextRand() % (kMineW - rw - 8));
        int ry = 1 + static_cast<int>(nextRand() % (kMineH - rh - 3));
        bool okRoom = true;
        for (int y = ry; y < ry + rh && okRoom; y++) {
            for (int x = rx; x < rx + rw && okRoom; x++) {
                if ((x < 6 && y < 6) || mine_[y][x] == 2 || mine_[y][x] == 3) okRoom = false;
            }
        }
        if (okRoom) {
            for (int y = ry; y < ry + rh; y++) {
                for (int x = rx; x < rx + rw; x++) {
                    bool ring = x == rx || x == rx + rw - 1 || y <= ry + 1 ||
                                y >= ry + rh - 2;
                    mine_[y][x] = ring ? 6 : 0;
                }
            }
            // The doorway pierces both south rows: a dark passage inside,
            // the lantern arch on the outer face.
            int doorX = rx + rw / 2;
            mine_[ry + rh - 2][doorX] = 7;
            mine_[ry + rh - 1][doorX] = 7;
            // Cut a path from the door south until it meets open cave.
            for (int y = ry + rh; y < kMineH - 1; y++) {
                if (mine_[y][doorX] != 1) break;
                mine_[y][doorX] = 0;
            }
        }
    }
    // A rail run with a cart, every other floor (flavor). The run bends
    // every few tiles so the corner track pieces actually see use - the
    // draw side picks straight/corner sprites from the connection mask.
    if (mineFloor_ % 2 == 0) {
        int x = 4 + static_cast<int>(nextRand() % (kMineW - 8));
        int y = 5 + static_cast<int>(nextRand() % (kMineH - 10));
        bool horiz = nextRand() % 2 == 0;
        int total = 9 + static_cast<int>(nextRand() % 4);
        int leg = 3 + static_cast<int>(nextRand() % 3);
        int cartX = -1, cartY = -1, laid = 0;
        for (int i = 0; i < total; i++) {
            if (x < 2 || y < 2 || x >= kMineW - 2 || y >= kMineH - 2) break;
            if (mine_[y][x] == 0) {
                mine_[y][x] = 4;
                laid++;
                if (laid == total / 2) {
                    cartX = x;
                    cartY = y;
                }
            }
            if (--leg == 0) {
                horiz = !horiz;
                leg = 3 + static_cast<int>(nextRand() % 3);
            }
            if (horiz) x++; else y++;
        }
        if (cartX >= 0 && mine_[cartY][cartX] == 4) mine_[cartY][cartX] = 5;
    }
    // Ore nodes, weighted by depth.
    int totalW = 0;
    for (const NodeInfo& n : kNodes) {
        if (mineFloor_ >= n.minFloor) totalW += n.weight;
    }
    int nodeCount = 8 + (mineFloor_ < 8 ? mineFloor_ : 8);
    for (int placed = 0, tries = 0; placed < nodeCount && tries < 400; tries++) {
        int x = 1 + static_cast<int>(nextRand() % (kMineW - 2));
        int y = 1 + static_cast<int>(nextRand() % (kMineH - 2));
        if (mine_[y][x] != 0 || (x < 6 && y < 6)) continue;
        int roll = static_cast<int>(nextRand() % static_cast<uint32_t>(totalW));
        int kind = 0;
        for (int i = 0; i < 8; i++) {
            if (mineFloor_ < kNodes[i].minFloor) continue;
            roll -= kNodes[i].weight;
            if (roll < 0) {
                kind = i;
                break;
            }
        }
        mine_[y][x] = static_cast<uint8_t>(10 + kind);
        placed++;
    }
    // Enemies: slimes everywhere, bats from floor 2. Deeper = more and tougher.
    foes_.clear();
    int foeCount = 2 + mineFloor_ * 2 / 3;
    if (foeCount > 8) foeCount = 8;
    int hpBonus = mineFloor_ / 4;
    for (int placed = 0, tries = 0; placed < foeCount && tries < 300; tries++) {
        int x = 1 + static_cast<int>(nextRand() % (kMineW - 2));
        int y = 1 + static_cast<int>(nextRand() % (kMineH - 2));
        if (mine_[y][x] != 0 || (x < 9 && y < 9)) continue;
        MineEnemy e = {};
        e.kind = (mineFloor_ >= 2 && nextRand() % 10 < 4) ? 1 : 0;
        // Deep down, half the bats are the small, fast kind.
        if (e.kind == 1 && mineFloor_ >= 6 && nextRand() % 2 == 0) e.kind = 2;
        e.x = static_cast<float>(x) + 0.5f;
        e.y = static_cast<float>(y) + 0.5f;
        e.hp = (e.kind == 0 ? 3 : (e.kind == 1 ? 2 : 1)) + hpBonus;
        e.aiT = 0.5f + static_cast<float>(nextRand() % 20) / 10.0f;
        foes_.push_back(e);
        placed++;
    }
}

bool WorldScene::mineBlocked(float x, float y) const {
    int tx = static_cast<int>(std::floor(x));
    int ty = static_cast<int>(std::floor(y));
    if (tx < 0 || ty < 0 || tx >= kMineW || ty >= kMineH) return true;
    uint8_t k = mine_[ty][tx];
    // Walls (boulder 1, brick 6), carts, ore nodes. Arch doors (7) pass.
    return k == 1 || k == 5 || k == 6 || k >= 10;
}

void WorldScene::hurtPlayer(float fromX, float fromY) {
    if (invulnT_ > 0.0f) return;
    hp_--;
    invulnT_ = core::kInvulnSec;
    float dx = minePos_.x - fromX;
    float dy = minePos_.y - fromY;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.01f) {
        pKnockX_ = dx / len * 7.0f;
        pKnockY_ = dy / len * 7.0f;
    }
    platform::playSfx(platform::Sfx::Hurt);
    if (hp_ <= 0) {
        exitMine("You blacked out... and woke at the shaft.");
    }
}

bool WorldScene::tryEat() {
    if (hp_ >= core::kMaxHp) return false;
    const core::ItemStack& sel = selectedStack();
    int heal = 0;
    switch (sel.item) {
        case core::kItemBerries: heal = 1; break;
        case core::kItemMushroom: heal = 1; break;
        case core::kItemApple:
        case core::kItemOrange:
        case core::kItemPear:
        case core::kItemPeach: heal = 2; break;
        case core::kItemEgg: heal = 1; break;
        case core::kItemMilk: heal = 2; break;
        case core::kItemHoney: heal = 3; break;
        case core::kItemFishSmall:
        case core::kItemShrimp:
        case core::kItemClownfish:
        case core::kItemSnail: heal = 2; break;
        case core::kItemFishMed:
        case core::kItemCrab:
        case core::kItemSeahorse:
        case core::kItemOctopus: heal = 3; break;
        case core::kItemFishBig:
        case core::kItemLobster:
        case core::kItemRay:
        case core::kItemTurtle: heal = 4; break;
        case core::kItemCorn: heal = 2; break;
        case core::kItemPotion: heal = 4; break;
        case core::kItemLettuce:
        case core::kItemRadish:
        case core::kItemCucumber:
        case core::kItemCauliflower:
        case core::kItemBeetroot:
        case core::kItemEggplant: heal = 1; break;
        case core::kItemStarfruit: heal = 3; break;
        default: return false;
    }
    if (!state_->inventory.remove(sel.item, 1)) return false;
    hp_ += heal;
    if (hp_ > core::kMaxHp) hp_ = core::kMaxHp;
    setStatus("Yum. +%d HP", heal);
    platform::playSfx(platform::Sfx::Eat);
    return true;
}

void WorldScene::attackSwing() {
    if (atkT_ > 0.0f) return;
    atkT_ = core::kAttackCooldownSec;
    actionTimer_ = 14;
    platform::playSfx(platform::Sfx::Hit);

    const core::ItemStack& sel = selectedStack();
    int dmg = (sel.item == core::kItemAxe || sel.item == core::kItemPickaxe) ? 2 : 1;
    core::Vec2f off = facingOffset();
    float cx = minePos_.x + off.x * 0.9f;
    float cy = minePos_.y + off.y * 0.9f;

    for (size_t i = 0; i < foes_.size();) {
        MineEnemy& e = foes_[i];
        float dx = e.x - cx;
        float dy = e.y - cy;
        if (dx * dx + dy * dy < 0.95f) {
            e.hp -= dmg;
            e.hurtT = 0.18f;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.01f) {
                e.vx = dx / len * 7.0f;
                e.vy = dy / len * 7.0f;
            }
            if (e.hp <= 0) {
                // Drops + Mining XP.
                if (e.kind == 0) {
                    uint32_t r = nextRand() % 100;
                    if (r < 12) state_->inventory.add(core::kItemPotion, 1);
                    else if (r < 45) state_->inventory.add(core::kItemCoal, 1);
                    else state_->inventory.add(core::kItemStone, 1);
                    awardXp(core::Skill::Mining, core::kXpKillSlime);
                } else if (e.kind == 1) {
                    if (nextRand() % 100 < 25) state_->inventory.add(core::kItemMushroom, 1);
                    awardXp(core::Skill::Mining, core::kXpKillBat);
                } else {
                    if (nextRand() % 100 < 40) state_->inventory.add(core::kItemCoal, 1);
                    awardXp(core::Skill::Mining, core::kXpKillBat);
                }
                platform::playSfx(platform::Sfx::Kill);
                foes_.erase(foes_.begin() + static_cast<long>(i));
                continue;
            }
        }
        i++;
    }
}

void WorldScene::doMineAction() {
    core::Vec2f off = facingOffset();
    int fx = static_cast<int>(std::floor(minePos_.x + off.x));
    int fy = static_cast<int>(std::floor(minePos_.y + off.y));
    if (fx < 0 || fy < 0 || fx >= kMineW || fy >= kMineH) return;
    uint8_t cell = mine_[fy][fx];

    if (cell == 3) { // exit portal: up one floor, or out
        if (mineFloor_ <= 1) {
            exitMine("Back to daylight!");
        } else {
            mineFloor_--;
            generateMineFloor();
            setStatus("Climbed up to floor %d", mineFloor_);
            platform::playSfx(platform::Sfx::Ui);
        }
        return;
    }
    if (cell == 2) { // hole down
        mineFloor_++;
        generateMineFloor();
        setStatus("Floor %d...", mineFloor_);
        platform::playSfx(platform::Sfx::Descend);
        return;
    }
    if (cell >= 10) { // ore node
        const NodeInfo& n = kNodes[cell - 10];
        const core::ItemStack& sel = selectedStack();
        int amount = n.dropCount + (sel.item == core::kItemPickaxe ? 1 : 0);
        state_->inventory.add(n.drop, amount);
        char extra[32] = {0};
        if (n.bonus != core::kItemNone) {
            state_->inventory.add(n.bonus, 1);
            snprintf(extra, sizeof(extra), " +1 %s", core::kItemTable[n.bonus].name);
        }
        if (nextRand() % 100 < 6) {
            state_->inventory.add(core::kItemPotion, 1);
            snprintf(extra, sizeof(extra), " +1 Potion!");
        }
        mine_[fy][fx] = 0;
        awardXp(core::Skill::Mining, n.xp);
        setStatus("+%d %s%s", amount, core::kItemTable[n.drop].name, extra);
        platform::playSfx(platform::Sfx::Mine);
        actionTimer_ = 14;
        return;
    }
    // Standing on the floor's rail run: hop in a minecart and ride it.
    int px = static_cast<int>(std::floor(minePos_.x));
    int py = static_cast<int>(std::floor(minePos_.y));
    if (px >= 0 && py >= 0 && px < kMineW && py < kMineH &&
        (mine_[py][px] == 4 || mine_[py][px] == 5)) {
        auto railTo = [&](int dx, int dy) {
            int x = px + dx, y = py + dy;
            return x >= 0 && y >= 0 && x < kMineW && y < kMineH &&
                   (mine_[y][x] == 4 || mine_[y][x] == 5);
        };
        int fdx = static_cast<int>(off.x), fdy = static_cast<int>(off.y);
        int dx = 0, dy = 0;
        if (railTo(fdx, fdy)) {
            dx = fdx;
            dy = fdy;
        } else {
            static const int kDirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
            for (const auto& d : kDirs) {
                if (railTo(d[0], d[1])) {
                    dx = d[0];
                    dy = d[1];
                    break;
                }
            }
        }
        if (dx != 0 || dy != 0) {
            riding_ = true;
            rideDirX_ = dx;
            rideDirY_ = dy;
            rideDecidedX_ = px;
            rideDecidedY_ = py;
            minePos_ = {static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f};
            state_->facing = dx > 0 ? core::Facing::Right
                             : dx < 0 ? core::Facing::Left
                             : dy < 0 ? core::Facing::Up
                                      : core::Facing::Down;
            setStatus("All aboard! (A or B to hop out)");
            platform::playSfx(platform::Sfx::Ui);
            return;
        }
    }
    // Eating beats swinging at air when hurt and holding food.
    if (tryEat()) return;
    attackSwing();
}

void WorldScene::updateMine(float dt, const platform::InputState& input) {
    if (invulnT_ > 0.0f) invulnT_ -= dt;
    if (atkT_ > 0.0f) atkT_ -= dt;

    // Riding the floor's rail run - same track-following logic as the
    // overworld, on the mine grid. Enemies still act, so it's a joyride
    // THROUGH danger, not around it. wasRiding also swallows the dismount
    // press so it can't trigger an action (and instantly remount).
    bool wasRiding = riding_;
    if (riding_) {
        if (input.actionPressed || input.cancelPressed) {
            riding_ = false;
            setStatus("Hopped out");
        } else {
            constexpr float kCartTilesPerSecond = 7.0f;
            minePos_.x += static_cast<float>(rideDirX_) * kCartTilesPerSecond * dt;
            minePos_.y += static_cast<float>(rideDirY_) * kCartTilesPerSecond * dt;
            int cx = static_cast<int>(std::floor(minePos_.x));
            int cy = static_cast<int>(std::floor(minePos_.y));
            auto railCell = [&](int x, int y) {
                return x >= 0 && y >= 0 && x < kMineW && y < kMineH &&
                       (mine_[y][x] == 4 || mine_[y][x] == 5);
            };
            if (cx != rideDecidedX_ || cy != rideDecidedY_) {
                float lx = minePos_.x - static_cast<float>(cx);
                float ly = minePos_.y - static_cast<float>(cy);
                bool crossed = (rideDirX_ > 0 && lx >= 0.5f) || (rideDirX_ < 0 && lx <= 0.5f) ||
                               (rideDirY_ > 0 && ly >= 0.5f) || (rideDirY_ < 0 && ly <= 0.5f);
                if (crossed) {
                    rideDecidedX_ = cx;
                    rideDecidedY_ = cy;
                    minePos_ = {static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f};
                    if (!railCell(cx + rideDirX_, cy + rideDirY_)) {
                        static const int kDirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
                        int ndx = 0, ndy = 0;
                        for (const auto& d : kDirs) {
                            if (d[0] == -rideDirX_ && d[1] == -rideDirY_) continue;
                            if (railCell(cx + d[0], cy + d[1])) {
                                ndx = d[0];
                                ndy = d[1];
                                break;
                            }
                        }
                        if (ndx == 0 && ndy == 0) {
                            riding_ = false;
                            setStatus("End of the line!");
                        } else {
                            rideDirX_ = ndx;
                            rideDirY_ = ndy;
                        }
                    }
                    if (riding_) {
                        state_->facing = rideDirX_ > 0   ? core::Facing::Right
                                         : rideDirX_ < 0 ? core::Facing::Left
                                         : rideDirY_ < 0 ? core::Facing::Up
                                                         : core::Facing::Down;
                    }
                }
            }
            moving_ = true;
        }
    }

    // Movement (same feel as the surface, plus hit-knockback) - the cart
    // drives instead while riding.
    if (!wasRiding) {
        core::Vec2f delta{0.0f, 0.0f};
        switch (input.move) {
            case platform::MoveDir::Up: delta = {0.0f, -1.0f}; state_->facing = core::Facing::Up; break;
            case platform::MoveDir::Down: delta = {0.0f, 1.0f}; state_->facing = core::Facing::Down; break;
            case platform::MoveDir::Left: delta = {-1.0f, 0.0f}; state_->facing = core::Facing::Left; break;
            case platform::MoveDir::Right: delta = {1.0f, 0.0f}; state_->facing = core::Facing::Right; break;
            case platform::MoveDir::None: break;
        }
        float mx = delta.x * kMoveTilesPerSecond + pKnockX_;
        float my = delta.y * kMoveTilesPerSecond + pKnockY_;
        float decay = 1.0f - 6.0f * dt;
        if (decay < 0.0f) decay = 0.0f;
        pKnockX_ *= decay;
        pKnockY_ *= decay;
        if (mx != 0.0f && !mineBlocked(minePos_.x + mx * dt, minePos_.y)) minePos_.x += mx * dt;
        if (my != 0.0f && !mineBlocked(minePos_.x, minePos_.y + my * dt)) minePos_.y += my * dt;
        moving_ = std::fabs(mx) + std::fabs(my) > 0.3f;
    }

    if (input.lPressed) cycleHudTab(-1);
    if (input.rPressed) cycleHudTab(1);
    if (input.touchTapped) {
        switch (hudTab_) {
            case HudTab::Inventory: handleInventoryTap(input.touchX, input.touchY); break;
            case HudTab::Skills: break;
        }
    }
    if (!wasRiding) {
        if (input.actionPressed) doMineAction();
        if (input.cancelPressed) attackSwing();
    }

    // Enemies.
    for (MineEnemy& e : foes_) {
        if (e.hurtT > 0.0f) e.hurtT -= dt;
        float pdx = minePos_.x - e.x;
        float pdy = minePos_.y - e.y;
        float pdist = std::sqrt(pdx * pdx + pdy * pdy);

        if (e.kind == 0) { // slime: hop toward the player in bursts
            e.aiT -= dt;
            if (e.hopping) {
                if (e.aiT <= 0.0f) {
                    e.hopping = false;
                    e.aiT = 0.6f + static_cast<float>(nextRand() % 15) / 10.0f;
                    e.vx = e.vy = 0.0f;
                }
            } else if (e.aiT <= 0.0f) {
                e.hopping = true;
                e.aiT = 0.5f;
                float tx, ty;
                if (pdist < 6.0f && pdist > 0.01f) {
                    tx = pdx / pdist;
                    ty = pdy / pdist;
                } else {
                    float ang = static_cast<float>(nextRand() % 628) / 100.0f;
                    tx = std::cos(ang);
                    ty = std::sin(ang);
                }
                e.vx = tx * 2.4f;
                e.vy = ty * 2.4f;
            }
        } else { // bats: constant swoop, ignore rocks (small ones are quick)
            float sp = e.kind == 2 ? 3.7f : 2.6f;
            if (pdist > 0.2f) {
                e.vx += (pdx / pdist) * 5.0f * dt;
                e.vy += (pdy / pdist) * 5.0f * dt;
            }
            float mag = std::sqrt(e.vx * e.vx + e.vy * e.vy);
            if (mag > sp) {
                e.vx *= sp / mag;
                e.vy *= sp / mag;
            }
            e.aiT += dt;
        }

        float nx = e.x + e.vx * dt;
        float ny = e.y + e.vy * dt;
        if (e.kind == 1) { // bats fly over everything but stay in bounds
            if (nx > 1.2f && nx < kMineW - 1.2f) e.x = nx;
            if (ny > 1.2f && ny < kMineH - 1.2f) e.y = ny;
        } else {
            if (!mineBlocked(nx, e.y)) e.x = nx; else e.vx = 0.0f;
            if (!mineBlocked(e.x, ny)) e.y = ny; else e.vy = 0.0f;
        }

        if (pdist < 0.55f) {
            hurtPlayer(e.x, e.y);
            // Blacking out clears foes_ - bail before touching the vector.
            if (mineFloor_ == 0) return;
        }
    }
}

// --- Sprites ------------------------------------------------------------------

int WorldScene::playerSprite() const {
    static const int kActs[4][2] = {
        {atlas_act_down_0_idx, atlas_act_down_1_idx},
        {atlas_act_up_0_idx, atlas_act_up_1_idx},
        {atlas_act_left_0_idx, atlas_act_left_1_idx},
        {atlas_act_right_0_idx, atlas_act_right_1_idx},
    };
    static const int kWalk[4][4] = {
        {atlas_player_down_0_idx, atlas_player_down_1_idx, atlas_player_down_2_idx, atlas_player_down_3_idx},
        {atlas_player_up_0_idx, atlas_player_up_1_idx, atlas_player_up_2_idx, atlas_player_up_3_idx},
        {atlas_player_left_0_idx, atlas_player_left_1_idx, atlas_player_left_2_idx, atlas_player_left_3_idx},
        {atlas_player_right_0_idx, atlas_player_right_1_idx, atlas_player_right_2_idx, atlas_player_right_3_idx},
    };
    static const int kSwim[4][2] = {
        {atlas_swim_down_0_idx, atlas_swim_down_1_idx},
        {atlas_swim_up_0_idx, atlas_swim_up_1_idx},
        {atlas_swim_left_0_idx, atlas_swim_left_1_idx},
        {atlas_swim_right_0_idx, atlas_swim_right_1_idx},
    };
    // Tool-matched action poses. Hoe/axe/watering are the sheet's baked
    // sets; hammer/pickaxe/rod are prep-time paperdoll composites (tool
    // icon on the toolless base); everything else swings empty-handed.
    // Adding new equipment = one EQUIP_TOOLS line in prep_assets.py plus
    // one case here.
    static const int kActsAxe[4][2] = {
        {atlas_act_axe_down_0_idx, atlas_act_axe_down_1_idx},
        {atlas_act_axe_up_0_idx, atlas_act_axe_up_1_idx},
        {atlas_act_axe_left_0_idx, atlas_act_axe_left_1_idx},
        {atlas_act_axe_right_0_idx, atlas_act_axe_right_1_idx},
    };
    static const int kActsWater[4][2] = {
        {atlas_act_water_down_0_idx, atlas_act_water_down_1_idx},
        {atlas_act_water_up_0_idx, atlas_act_water_up_1_idx},
        {atlas_act_water_left_0_idx, atlas_act_water_left_1_idx},
        {atlas_act_water_right_0_idx, atlas_act_water_right_1_idx},
    };
    static const int kActsBase[4][2] = {
        {atlas_act_base_down_0_idx, atlas_act_base_down_1_idx},
        {atlas_act_base_up_0_idx, atlas_act_base_up_1_idx},
        {atlas_act_base_left_0_idx, atlas_act_base_left_1_idx},
        {atlas_act_base_right_0_idx, atlas_act_base_right_1_idx},
    };
    static const int kActsHammer[4][2] = {
        {atlas_act_hammer_down_0_idx, atlas_act_hammer_down_1_idx},
        {atlas_act_hammer_up_0_idx, atlas_act_hammer_up_1_idx},
        {atlas_act_hammer_left_0_idx, atlas_act_hammer_left_1_idx},
        {atlas_act_hammer_right_0_idx, atlas_act_hammer_right_1_idx},
    };
    static const int kActsPick[4][2] = {
        {atlas_act_pick_down_0_idx, atlas_act_pick_down_1_idx},
        {atlas_act_pick_up_0_idx, atlas_act_pick_up_1_idx},
        {atlas_act_pick_left_0_idx, atlas_act_pick_left_1_idx},
        {atlas_act_pick_right_0_idx, atlas_act_pick_right_1_idx},
    };
    static const int kActsRod[4][2] = {
        {atlas_act_rod_down_0_idx, atlas_act_rod_down_1_idx},
        {atlas_act_rod_up_0_idx, atlas_act_rod_up_1_idx},
        {atlas_act_rod_left_0_idx, atlas_act_rod_left_1_idx},
        {atlas_act_rod_right_0_idx, atlas_act_rod_right_1_idx},
    };
    // Dedicated rod-out waiting pose while the line is in the water
    // (Ocean-pack fishing sheets; the cast swing itself is act_rod).
    static const int kFishWait[4][2] = {
        {atlas_fishwait_down_0_idx, atlas_fishwait_down_1_idx},
        {atlas_fishwait_up_0_idx, atlas_fishwait_up_1_idx},
        {atlas_fishwait_left_0_idx, atlas_fishwait_left_1_idx},
        {atlas_fishwait_right_0_idx, atlas_fishwait_right_1_idx},
    };
    int dir = static_cast<int>(state_->facing);
    if (swimming_ && mineFloor_ == 0) {
        return kSwim[dir][(animFrame_ / 12) % 2];
    }
    if (fishState_ != 0) {
        return kFishWait[dir][(animFrame_ / 16) % 2];
    }
    if (actionTimer_ > 0) {
        const int(*set)[2];
        switch (selectedStack().item) {
            case core::kItemHoe: set = kActs; break;
            case core::kItemAxe: set = kActsAxe; break;
            case core::kItemPickaxe: set = kActsPick; break;
            case core::kItemHammer: set = kActsHammer; break;
            case core::kItemFishingRod: set = kActsRod; break;
            case core::kItemWateringCan:
            case core::kItemWateringCanFull: set = kActsWater; break;
            default: set = kActsBase; break;
        }
        return set[dir][actionTimer_ > 10 ? 0 : 1];
    }
    if (moving_) {
        return kWalk[dir][2 + (animFrame_ / 8) % 2];
    }
    return kWalk[dir][(animFrame_ / 32) % 2];
}

int WorldScene::spriteForCropStage(uint8_t speciesId, int stage, bool watered) {
    // Watered twins: same art with the soil mound darkened wet (generated
    // by prep_assets.py's wetten(), mirroring the pack's own watered set).
    static const int kStagesW[core::kCropSpeciesCount][4] = {
        {atlas_crop_wheat_0w_idx, atlas_crop_wheat_1w_idx, atlas_crop_wheat_2w_idx, atlas_crop_wheat_3w_idx},
        {atlas_crop_turnip_0w_idx, atlas_crop_turnip_1w_idx, atlas_crop_turnip_2w_idx, atlas_crop_turnip_3w_idx},
        {atlas_crop_carrot_0w_idx, atlas_crop_carrot_1w_idx, atlas_crop_carrot_2w_idx, atlas_crop_carrot_3w_idx},
        {atlas_crop_tomato_0w_idx, atlas_crop_tomato_1w_idx, atlas_crop_tomato_2w_idx, atlas_crop_tomato_3w_idx},
        {atlas_crop_pumpkin_0w_idx, atlas_crop_pumpkin_1w_idx, atlas_crop_pumpkin_2w_idx, atlas_crop_pumpkin_3w_idx},
        {atlas_crop_cauliflower_0w_idx, atlas_crop_cauliflower_1w_idx, atlas_crop_cauliflower_2w_idx, atlas_crop_cauliflower_3w_idx},
        {atlas_crop_eggplant_0w_idx, atlas_crop_eggplant_1w_idx, atlas_crop_eggplant_2w_idx, atlas_crop_eggplant_3w_idx},
        {atlas_crop_lettuce_0w_idx, atlas_crop_lettuce_1w_idx, atlas_crop_lettuce_2w_idx, atlas_crop_lettuce_3w_idx},
        {atlas_crop_radish_0w_idx, atlas_crop_radish_1w_idx, atlas_crop_radish_2w_idx, atlas_crop_radish_3w_idx},
        {atlas_crop_beetroot_0w_idx, atlas_crop_beetroot_1w_idx, atlas_crop_beetroot_2w_idx, atlas_crop_beetroot_3w_idx},
        {atlas_crop_starfruit_0w_idx, atlas_crop_starfruit_1w_idx, atlas_crop_starfruit_2w_idx, atlas_crop_starfruit_3w_idx},
        {atlas_crop_cucumber_0w_idx, atlas_crop_cucumber_1w_idx, atlas_crop_cucumber_2w_idx, atlas_crop_cucumber_3w_idx},
        {atlas_crop_corn_0w_idx, atlas_crop_corn_1w_idx, atlas_crop_corn_2w_idx, atlas_crop_corn_3w_idx},
        {atlas_crop_sunflower_0w_idx, atlas_crop_sunflower_1w_idx, atlas_crop_sunflower_2w_idx, atlas_crop_sunflower_3w_idx},
    };
    static const int kStages[core::kCropSpeciesCount][4] = {
        {atlas_crop_wheat_0_idx, atlas_crop_wheat_1_idx, atlas_crop_wheat_2_idx, atlas_crop_wheat_3_idx},
        {atlas_crop_turnip_0_idx, atlas_crop_turnip_1_idx, atlas_crop_turnip_2_idx, atlas_crop_turnip_3_idx},
        {atlas_crop_carrot_0_idx, atlas_crop_carrot_1_idx, atlas_crop_carrot_2_idx, atlas_crop_carrot_3_idx},
        {atlas_crop_tomato_0_idx, atlas_crop_tomato_1_idx, atlas_crop_tomato_2_idx, atlas_crop_tomato_3_idx},
        {atlas_crop_pumpkin_0_idx, atlas_crop_pumpkin_1_idx, atlas_crop_pumpkin_2_idx, atlas_crop_pumpkin_3_idx},
        {atlas_crop_cauliflower_0_idx, atlas_crop_cauliflower_1_idx, atlas_crop_cauliflower_2_idx, atlas_crop_cauliflower_3_idx},
        {atlas_crop_eggplant_0_idx, atlas_crop_eggplant_1_idx, atlas_crop_eggplant_2_idx, atlas_crop_eggplant_3_idx},
        {atlas_crop_lettuce_0_idx, atlas_crop_lettuce_1_idx, atlas_crop_lettuce_2_idx, atlas_crop_lettuce_3_idx},
        {atlas_crop_radish_0_idx, atlas_crop_radish_1_idx, atlas_crop_radish_2_idx, atlas_crop_radish_3_idx},
        {atlas_crop_beetroot_0_idx, atlas_crop_beetroot_1_idx, atlas_crop_beetroot_2_idx, atlas_crop_beetroot_3_idx},
        {atlas_crop_starfruit_0_idx, atlas_crop_starfruit_1_idx, atlas_crop_starfruit_2_idx, atlas_crop_starfruit_3_idx},
        {atlas_crop_cucumber_0_idx, atlas_crop_cucumber_1_idx, atlas_crop_cucumber_2_idx, atlas_crop_cucumber_3_idx},
        {atlas_crop_corn_0_idx, atlas_crop_corn_1_idx, atlas_crop_corn_2_idx, atlas_crop_corn_3_idx},
        {atlas_crop_sunflower_0_idx, atlas_crop_sunflower_1_idx, atlas_crop_sunflower_2_idx, atlas_crop_sunflower_3_idx},
    };
    if (speciesId >= core::kCropSpeciesCount) speciesId = 0;
    if (stage < 0) stage = 0;
    if (stage > 3) stage = 3;
    return watered ? kStagesW[speciesId][stage] : kStages[speciesId][stage];
}

int WorldScene::spriteForItem(core::ItemId item) {
    switch (item) {
        case core::kItemHoe: return atlas_tool_hoe_idx;
        case core::kItemAxe: return atlas_tool_axe_idx;
        case core::kItemPickaxe: return atlas_tool_pickaxe_idx;
        case core::kItemHammer: return atlas_tool_hammer_idx;
        case core::kItemWateringCan: return atlas_tool_watering_can_idx;
        case core::kItemWateringCanFull: return atlas_tool_watering_can_full_idx;
        case core::kItemFishingRod: return atlas_tool_rod_idx;
        case core::kItemWheatSeed: return atlas_crop_wheat_seed_idx;
        case core::kItemWheat: return atlas_crop_wheat_harvested_idx;
        case core::kItemTurnipSeed: return atlas_crop_turnip_seed_idx;
        case core::kItemTurnip: return atlas_crop_turnip_harvested_idx;
        case core::kItemCarrotSeed: return atlas_crop_carrot_seed_idx;
        case core::kItemCarrot: return atlas_crop_carrot_harvested_idx;
        case core::kItemTomatoSeed: return atlas_crop_tomato_seed_idx;
        case core::kItemTomato: return atlas_crop_tomato_harvested_idx;
        case core::kItemPumpkinSeed: return atlas_crop_pumpkin_seed_idx;
        case core::kItemPumpkin: return atlas_crop_pumpkin_harvested_idx;
        case core::kItemWood: return atlas_item_wood_idx;
        case core::kItemStone: return atlas_item_stone_idx;
        case core::kItemOre: return atlas_item_ore_idx;
        case core::kItemSapling: return atlas_item_sapling_idx;
        case core::kItemBerries: return atlas_item_berries_idx;
        case core::kItemMushroom: return atlas_prop_mushroom_idx;
        case core::kItemHay: return atlas_item_hay_idx;
        case core::kItemApple: return atlas_item_apple_idx;
        case core::kItemEgg: return atlas_item_egg_idx;
        case core::kItemMilk: return atlas_item_milk_idx;
        case core::kItemHoney: return atlas_item_honey_idx;
        case core::kItemFishSmall: return atlas_item_fish_small_idx;
        case core::kItemFishMed: return atlas_item_fish_med_idx;
        case core::kItemFishBig: return atlas_item_fish_big_idx;
        case core::kItemFence: return atlas_at_fence_0_idx;
        case core::kItemRoof: return atlas_roof_m_idx;
        case core::kItemGate: return atlas_place_gate_l_idx;
        case core::kItemGateRight: return atlas_place_gate_r_idx;
        case core::kItemPath: return atlas_tile_path_idx;
        case core::kItemPathDirt: return atlas_at_path_15_idx;
        case core::kItemPathPlank: return atlas_path_planks_idx;
        case core::kItemRail: return atlas_rail_h_idx;
        case core::kItemFloor: return atlas_at_floor_15_idx;
        case core::kItemBridge: return atlas_tile_bridge_idx;
        case core::kItemCamp: return atlas_place_camp_idx;
        case core::kItemCoop: return atlas_place_coop_idx;
        case core::kItemBarn: return atlas_place_barn_idx;
        case core::kItemChest: return atlas_chest_oak_idx;
        case core::kItemWell: return atlas_place_well_idx;
        case core::kItemBeehive: return atlas_place_beehive_idx;
        case core::kItemCampfire: return atlas_place_campfire_idx;
        case core::kItemLamp: return atlas_place_lamp_idx;
        case core::kItemChair: return atlas_chair_0_idx;
        case core::kItemRug: return atlas_rug_s_0_idx;
        case core::kItemRugLong: return atlas_rug_l_0_idx;
        case core::kItemBed: return atlas_bed_0_idx;
        case core::kItemWoodTable: return atlas_table_w0_idx;
        case core::kItemDresser: return atlas_dresser_w0_idx;
        case core::kItemStool: return atlas_stool_w0_idx;
        case core::kItemBench: return atlas_bench_w0_idx;
        case core::kItemWorkbench: return atlas_place_workbench_idx;
        case core::kItemSign: return atlas_place_sign_idx;
        case core::kItemMailbox: return atlas_place_mailbox_idx;
        case core::kItemCoal: return atlas_item_coal_idx;
        case core::kItemRuby: return atlas_item_ruby_idx;
        case core::kItemDiamond: return atlas_item_diamond_idx;
        case core::kItemEmerald: return atlas_item_emerald_idx;
        case core::kItemAmethyst: return atlas_item_amethyst_idx;
        case core::kItemPotion: return atlas_item_potion_idx;
        case core::kItemMineShaft: return atlas_place_mineshaft_idx;
        case core::kItemOrange: return atlas_item_orange_idx;
        case core::kItemPear: return atlas_item_pear_idx;
        case core::kItemPeach: return atlas_item_peach_idx;
        case core::kItemCauliflowerSeed: return atlas_crop_cauliflower_seed_idx;
        case core::kItemCauliflower: return atlas_crop_cauliflower_harvested_idx;
        case core::kItemEggplantSeed: return atlas_crop_eggplant_seed_idx;
        case core::kItemEggplant: return atlas_crop_eggplant_harvested_idx;
        case core::kItemLettuceSeed: return atlas_crop_lettuce_seed_idx;
        case core::kItemLettuce: return atlas_crop_lettuce_harvested_idx;
        case core::kItemRadishSeed: return atlas_crop_radish_seed_idx;
        case core::kItemRadish: return atlas_crop_radish_harvested_idx;
        case core::kItemBeetrootSeed: return atlas_crop_beetroot_seed_idx;
        case core::kItemBeetroot: return atlas_crop_beetroot_harvested_idx;
        case core::kItemStarfruitSeed: return atlas_crop_starfruit_seed_idx;
        case core::kItemStarfruit: return atlas_crop_starfruit_harvested_idx;
        case core::kItemCucumberSeed: return atlas_crop_cucumber_seed_idx;
        case core::kItemCucumber: return atlas_crop_cucumber_harvested_idx;
        case core::kItemCornSeed: return atlas_crop_corn_seed_idx;
        case core::kItemCorn: return atlas_crop_corn_harvested_idx;
        case core::kItemShrimp: return atlas_item_shrimp_idx;
        case core::kItemClownfish: return atlas_item_clownfish_idx;
        case core::kItemSnail: return atlas_item_snail_idx;
        case core::kItemCrab: return atlas_item_crab_idx;
        case core::kItemSeahorse: return atlas_item_seahorse_idx;
        case core::kItemOctopus: return atlas_item_octopus_idx;
        case core::kItemLobster: return atlas_item_lobster_idx;
        case core::kItemRay: return atlas_item_ray_idx;
        case core::kItemTurtle: return atlas_item_turtle_idx;
        case core::kItemCottage: return atlas_place_cottage_idx;
        case core::kItemHut: return atlas_place_hut_idx;
        case core::kItemManor: return atlas_place_manor_idx;
        case core::kItemSnowman: return atlas_place_snowman_idx;
        case core::kItemTrough: return atlas_place_trough_idx;
        case core::kItemHayBale: return atlas_place_haybale_idx;
        case core::kItemWaterTray: return atlas_watertray_2_idx;
        case core::kItemBoat: return atlas_place_boat_idx;
        case core::kItemPicnic: return atlas_place_picnic_idx;
        case core::kItemPresent: return atlas_place_present_0_idx;
        case core::kItemChestSilver: return atlas_chest_silver_idx;
        case core::kItemChestGold: return atlas_chest_gold_idx;
        case core::kItemClone: return atlas_player_down_0_idx;
        case core::kItemSunflowerSeed: return atlas_crop_sunflower_seed_idx;
        case core::kItemSunflower: return atlas_crop_sunflower_harvested_idx;
        default: return atlas_tile_grass_0_idx;
    }
}

float WorldScene::iconScaleForItem(core::ItemId item) {
    switch (item) {
        case core::kItemCamp:
        case core::kItemCoop:
        case core::kItemBarn:
            return 0.8f; // 48px source in a 40px slot
        case core::kItemChest:
        case core::kItemRugLong:
        case core::kItemWell:
        case core::kItemBeehive:
        case core::kItemMailbox: // 16x32 - scale by its tall side
        case core::kItemWall:
        case core::kItemDoor:
        case core::kItemRoof:
        case core::kItemChair:
        case core::kItemBed:
        case core::kItemWoodTable:
        case core::kItemDresser:
        case core::kItemStool:
        case core::kItemBench:
        case core::kItemWorkbench:
        case core::kItemMineShaft:
            return 1.2f; // 32px source
        case core::kItemXmasTree:
            return 0.6f; // 48x64 source
        case core::kItemChestSilver:
        case core::kItemChestGold:
            return 1.2f; // 32px source
        case core::kItemWaterTray:
            return 1.2f; // 32x16 - scale by its wide side
        case core::kItemBoat:
        case core::kItemPicnic:
            return 0.8f; // 48px-wide sources
        case core::kItemCottage:
        case core::kItemHut:
            return 0.55f; // 64x64 source
        case core::kItemManor:
            return 0.4f; // 96x80 source
        case core::kItemSnowman:
            return 1.1f; // 16x32 source
        case core::kItemClone:
            return 1.5f; // 24x24 player frame
        default:
            return 2.0f; // 16px source
    }
}

void WorldScene::footprintOffsetForItem(core::ItemId item, float* offX, float* offY) {
    switch (item) {
        case core::kItemCamp:
        case core::kItemCoop:
        case core::kItemBarn:
            *offX = -32.0f;
            *offY = -64.0f;
            break;
        case core::kItemChest:
        case core::kItemChestSilver:
        case core::kItemChestGold:
        case core::kItemWell:
        case core::kItemBeehive:
        case core::kItemMineShaft:
            *offX = -16.0f;
            *offY = -32.0f;
            break;
        case core::kItemBoat:
            // 48x16 canvas, bottom-centered on the anchor tile.
            *offX = -32.0f;
            *offY = 0.0f;
            break;
        case core::kItemPicnic:
            // 48x32 spread, anchor tile at its south-center.
            *offX = -32.0f;
            *offY = -32.0f;
            break;
        case core::kItemWaterTray:
            // 32x16 - sits flush, spilling onto the east tile.
            *offX = 0.0f;
            *offY = 0.0f;
            break;
        case core::kItemMailbox:
        case core::kItemWall:
        case core::kItemDoor:
        case core::kItemRoof:
        case core::kItemChair:
        case core::kItemBed:
        case core::kItemWoodTable:
        case core::kItemDresser:
        case core::kItemStool:
        case core::kItemBench:
        case core::kItemWorkbench:
            *offX = 0.0f;
            *offY = -32.0f;
            break;
        case core::kItemXmasTree:
            *offX = -32.0f;
            *offY = -96.0f;
            break;
        case core::kItemCottage:
        case core::kItemHut:
            *offX = -48.0f;
            *offY = -96.0f;
            break;
        case core::kItemManor:
            // 96x80 source at 2x = 192x160 on screen, bottom-centered on
            // the anchor tile.
            *offX = -80.0f;
            *offY = -128.0f;
            break;
        case core::kItemSnowman:
            *offX = 0.0f;
            *offY = -32.0f;
            break;
        default:
            *offX = 0.0f;
            *offY = 0.0f;
            break;
    }
}

// --- Drawing ------------------------------------------------------------------

void WorldScene::draw(const platform::Renderer& renderer) const {
    if (!state_) return;

    if (paused_) {
        drawPauseMenu(renderer);
        return;
    }

    char aLbl[24], bLbl[24];
    contextPrompts(aLbl, sizeof(aLbl), bLbl, sizeof(bLbl));
    for (int eye = 0; eye < 2; eye++) {
        if (mineFloor_ > 0) {
            renderer.beginTop(eye, C2D_Color32(0x14, 0x0e, 0x0c, 0xFF));
            drawMine(renderer, eye);
        } else {
            renderer.beginTop(eye, C2D_Color32(0x30, 0x40, 0x20, 0xFF));
            drawWorld(renderer, eye);
            if (buildModeActive()) drawBuildGhost(renderer, eye);
            else drawSeedGhost(renderer, eye);
            drawPromptBar(renderer, eye, aLbl, bLbl);
        }
    }
    if (chestOpen_) {
        drawChestUi(renderer);
    } else if (craftOpen_) {
        drawCraftUi(renderer);
    } else {
        drawHud(renderer);
    }
}

namespace {

// core/wall_autotile's view of the mine grid: open = not a wall kind
// (1 = boulder, 6 = brick); out of bounds reads as solid rock, so the map
// border draws as thick mountain exactly as before.
struct WallOpenCtx {
    const uint8_t* cells;
    int32_t w, h;
};
bool wallOpenAt(const void* ctxp, int32_t x, int32_t y) {
    const WallOpenCtx* c = static_cast<const WallOpenCtx*>(ctxp);
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return false;
    uint8_t k = c->cells[y * c->w + x];
    return k != 1 && k != 6;
}

// Role -> atlas sprite for either kit. The brick kit is the boulder layout
// ten sheet rows up (see tools/prep_assets.py), so every role has a twin.
int wallSpriteFor(core::wallauto::Role role, bool brick) {
    using Role = core::wallauto::Role;
    static const int kDWall[16] = {
        atlas_at_dwall_0_idx,  atlas_at_dwall_1_idx,  atlas_at_dwall_2_idx,
        atlas_at_dwall_3_idx,  atlas_at_dwall_4_idx,  atlas_at_dwall_5_idx,
        atlas_at_dwall_6_idx,  atlas_at_dwall_7_idx,  atlas_at_dwall_8_idx,
        atlas_at_dwall_9_idx,  atlas_at_dwall_10_idx, atlas_at_dwall_11_idx,
        atlas_at_dwall_12_idx, atlas_at_dwall_13_idx, atlas_at_dwall_14_idx,
        atlas_at_dwall_15_idx};
    static const int kBWall[16] = {
        atlas_at_bwall_0_idx,  atlas_at_bwall_1_idx,  atlas_at_bwall_2_idx,
        atlas_at_bwall_3_idx,  atlas_at_bwall_4_idx,  atlas_at_bwall_5_idx,
        atlas_at_bwall_6_idx,  atlas_at_bwall_7_idx,  atlas_at_bwall_8_idx,
        atlas_at_bwall_9_idx,  atlas_at_bwall_10_idx, atlas_at_bwall_11_idx,
        atlas_at_bwall_12_idx, atlas_at_bwall_13_idx, atlas_at_bwall_14_idx,
        atlas_at_bwall_15_idx};
    if (role <= Role::W15) return (brick ? kBWall : kDWall)[static_cast<int>(role)];
    switch (role) {
        case Role::R0: return brick ? atlas_btop_s_idx : atlas_dtop_s_idx;
        case Role::R1: return brick ? atlas_bcap_m_idx : atlas_dcap_m_idx;
        case Role::R2: return brick ? atlas_btop_se_idx : atlas_dtop_se_idx;
        case Role::R2W: return brick ? atlas_btop_se_w_idx : atlas_dtop_se_w_idx;
        case Role::R3: return brick ? atlas_bcap_e_idx : atlas_dcap_e_idx;
        case Role::R8: return brick ? atlas_btop_sw_idx : atlas_dtop_sw_idx;
        case Role::R8J: return brick ? atlas_btop_sw_j_idx : atlas_dtop_sw_j_idx;
        case Role::R9: return brick ? atlas_bcap_w_idx : atlas_dcap_w_idx;
        case Role::RcapL: return brick ? atlas_bcap_l_idx : atlas_dcap_l_idx;
        case Role::RcapM: return brick ? atlas_bcap_m_idx : atlas_dcap_m_idx;
        case Role::RcapR: return brick ? atlas_bcap_r_idx : atlas_dcap_r_idx;
        case Role::Fcl: return brick ? atlas_bface_cl_idx : atlas_dface_cl_idx;
        case Role::Fcr: return brick ? atlas_bface_cr_idx : atlas_dface_cr_idx;
        case Role::FaceBoth: return brick ? atlas_bface_cb_idx : atlas_dface_cb_idx;
        // ccne/kse and ccnw/ksw are the same tile under two names (corner cap
        // vs bottom corner), exactly as in the lab's mapping.
        case Role::Ccne: case Role::Kse:
            return brick ? atlas_bcorner_se_idx : atlas_dcorner_se_idx;
        case Role::Ccnw: case Role::Ksw:
            return brick ? atlas_bcorner_sw_idx : atlas_dcorner_sw_idx;
        case Role::Ccne2: return brick ? atlas_btop_se2_idx : atlas_dtop_se2_idx;
        case Role::Ccnw2: return brick ? atlas_btop_sw2_idx : atlas_dtop_sw2_idx;
        case Role::CapBoth: case Role::Ks2:
            return brick ? atlas_bcorner_s2_idx : atlas_dcorner_s2_idx;
        case Role::Knw: return brick ? atlas_bcorner_nw_idx : atlas_dcorner_nw_idx;
        case Role::Kne: return brick ? atlas_bcorner_ne_idx : atlas_dcorner_ne_idx;
        case Role::Kn2: return brick ? atlas_bcorner_n2_idx : atlas_dcorner_n2_idx;
        case Role::Knw2: return brick ? atlas_bcorner_nw2_idx : atlas_dcorner_nw2_idx;
        case Role::Kne2: return brick ? atlas_bcorner_ne2_idx : atlas_dcorner_ne2_idx;
        case Role::CapE2: return brick ? atlas_bcap_e2_idx : atlas_dcap_e2_idx;
        case Role::CapW2: return brick ? atlas_bcap_w2_idx : atlas_dcap_w2_idx;
        case Role::RimE2: return brick ? atlas_brim_e2_idx : atlas_drim_e2_idx;
        case Role::RimW2: return brick ? atlas_brim_w2_idx : atlas_drim_w2_idx;
        default: return brick ? atlas_at_bwall_0_idx : atlas_at_dwall_0_idx;
    }
}

} // namespace

void WorldScene::drawMine(const platform::Renderer& renderer, int eye) const {
    float camX = minePos_.x;
    float camY = minePos_.y;
    // Clamp the camera to the room so the void never shows.
    float halfW = kTopScreenW / 2.0f / kScreenTilePx;
    float halfH = kTopScreenH / 2.0f / kScreenTilePx;
    if (camX < halfW) camX = halfW;
    if (camX > kMineW - halfW) camX = kMineW - halfW;
    if (camY < halfH) camY = halfH;
    if (camY > kMineH - halfH) camY = kMineH - halfH;

    static const int kFloorVariantsL[4] = {atlas_mine_floor_0_idx, atlas_mine_floor_1_idx,
                                           atlas_mine_floor_2_idx, atlas_mine_floor_3_idx};
    static const int kFloorVariantsD[4] = {atlas_mine_floor_d0_idx, atlas_mine_floor_d1_idx,
                                           atlas_mine_floor_d2_idx, atlas_mine_floor_d3_idx};
    // Dungeon-ground blob set: floor cells autotile against the walls so
    // rooms and corridors read as carved out of the rock. Deep floors
    // (5+) switch to the pack's darker ground recolor.
    static const int kAtMineL[16] = {
        atlas_at_mine_0_idx,  atlas_at_mine_1_idx,  atlas_at_mine_2_idx,  atlas_at_mine_3_idx,
        atlas_at_mine_4_idx,  atlas_at_mine_5_idx,  atlas_at_mine_6_idx,  atlas_at_mine_7_idx,
        atlas_at_mine_8_idx,  atlas_at_mine_9_idx,  atlas_at_mine_10_idx, atlas_at_mine_11_idx,
        atlas_at_mine_12_idx, atlas_at_mine_13_idx, atlas_at_mine_14_idx, atlas_at_mine_15_idx};
    static const int kAtMineD[16] = {
        atlas_at_mined_0_idx,  atlas_at_mined_1_idx,  atlas_at_mined_2_idx,  atlas_at_mined_3_idx,
        atlas_at_mined_4_idx,  atlas_at_mined_5_idx,  atlas_at_mined_6_idx,  atlas_at_mined_7_idx,
        atlas_at_mined_8_idx,  atlas_at_mined_9_idx,  atlas_at_mined_10_idx, atlas_at_mined_11_idx,
        atlas_at_mined_12_idx, atlas_at_mined_13_idx, atlas_at_mined_14_idx, atlas_at_mined_15_idx};
    bool deep = mineFloor_ >= 5;
    const int* kFloorVariants = deep ? kFloorVariantsD : kFloorVariantsL;
    const int* kAtMine = deep ? kAtMineD : kAtMineL;
    static const int kNodeSprites[8] = {atlas_node_stone_idx, atlas_node_coal_idx,
                                        atlas_node_copper_idx, atlas_node_gold_idx,
                                        atlas_node_ruby_idx, atlas_node_diamond_idx,
                                        atlas_node_emerald_idx, atlas_node_amethyst_idx};

    int32_t tileMinX = static_cast<int32_t>(std::floor(camX)) - 7;
    int32_t tileMaxX = static_cast<int32_t>(std::floor(camX)) + 7;
    int32_t tileMinY = static_cast<int32_t>(std::floor(camY)) - 5;
    int32_t tileMaxY = static_cast<int32_t>(std::floor(camY)) + 5;

    // Ground + walls + rails. Out-of-bounds tiles render as solid rock,
    // so the map border reads as thick mountain (dark tops above the
    // border faces, corners wrapping) instead of raw void.
    for (int32_t ty = tileMinY; ty <= tileMaxY; ty++) {
        for (int32_t tx = tileMinX; tx <= tileMaxX; tx++) {
            bool oob = tx < 0 || ty < 0 || tx >= kMineW || ty >= kMineH;
            float sx = (static_cast<float>(tx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float sy = (static_cast<float>(ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
            uint8_t k = oob ? 1 : mine_[ty][tx];
            auto openCell = [&](int32_t x, int32_t y) {
                return x >= 0 && y >= 0 && x < kMineW && y < kMineH && mine_[y][x] != 1 &&
                       mine_[y][x] != 6;
            };
            if (k == 1 || k == 6) {
                // Walls, both materials, ONE code path. The geometry -> role
                // decision lives in core/wall_autotile: the round-4 walk-
                // behind rule, derived in tools/wall_lab from the user's
                // per-cell corrections and pinned to the lab's output by
                // tests/test_wall_autotile (3078 cells, exact match). This
                // branch only maps roles to atlas sprites and rolls the
                // cosmetic face/decal hashes.
                bool brick = k == 6;
                using core::wallauto::Role;
                const WallOpenCtx octx{&mine_[0][0], kMineW, kMineH};
                // Floor under the wall first: rim, corner, and cap tiles have
                // transparent edges the ground shows through. Complement
                // rule - the floor autotile indexed by the WALL-side bits -
                // matching the lab render the user accepted.
                renderer.drawSprite(
                    kAtMine[15 - core::wallauto::baseMask(wallOpenAt, &octx, tx, ty)], sx, sy,
                    0.0f, eye, kSpriteScale);
                const core::wallauto::Cell cell =
                    core::wallauto::computeWallCell(wallOpenAt, &octx, tx, ty);
                Role role = core::wallauto::wbRoleForCell(wallOpenAt, &octx, tx, ty, cell);
                int spr = wallSpriteFor(role, brick);
                // Faces (em 4, and em 5 which renders as a face) roll the
                // occasional variant so long runs don't read as a flat tile
                // grid: sprout/pocked boulders, window/moss/crack bricks.
                if (role == Role::W4 && !cell.isCap && !cell.isCorner) {
                    if (brick) {
                        uint32_t fh = visHash(tx * 3 + 1, ty * 7 + mineFloor_) % 20;
                        if (fh == 0) spr = atlas_bface_window_idx;
                        else if (fh == 1) spr = atlas_bface_moss_idx;
                        else if (fh == 2) spr = atlas_bface_crack_idx;
                    } else {
                        static const int kDFaces[4] = {atlas_dface1_idx, atlas_dface2_idx,
                                                       atlas_dface3_idx, atlas_dface4_idx};
                        uint32_t fh = visHash(tx * 5 + 2, ty * 11 + mineFloor_) % 24;
                        if (fh < 4) spr = kDFaces[fh];
                    }
                }
                renderer.drawSprite(spr, sx, sy, 0.0f, eye, kSpriteScale);
                // Crowns and walk-behind ridges/rims are NOT drawn here
                // anymore: their entire point is occluding the player, so
                // they render in the sorted entity pass below, anchored at
                // the casting wall's base row.
                if (cell.em == 0 && !cell.isCap && !cell.isCorner) {
                    uint32_t bh = visHash(tx * 9 + 4, ty * 7 + mineFloor_);
                    if (bh % 11 == 0) {
                        static const int kDecals[6] = {
                            atlas_decal_bone_idx,   atlas_decal_bone2_idx,
                            atlas_decal_skull_idx,  atlas_decal_spark_idx,
                            atlas_decal_spark2_idx, atlas_decal_pebbles_idx};
                        renderer.drawSprite(kDecals[(bh / 11) % 6], sx, sy, 0.01f, eye,
                                            kSpriteScale);
                    }
                }
                continue;
            }
            // Floor cells autotile against walls (and the room border);
            // diagonal-only wall contacts get inner-corner notches so
            // wall clusters and crossings read rounded all the way around.
            static const int kAtMineNL[16] = {
                atlas_at_mine_15_idx,  atlas_at_mine_n1_idx,  atlas_at_mine_n2_idx,
                atlas_at_mine_n3_idx,  atlas_at_mine_n4_idx,  atlas_at_mine_n5_idx,
                atlas_at_mine_n6_idx,  atlas_at_mine_n7_idx,  atlas_at_mine_n8_idx,
                atlas_at_mine_n9_idx,  atlas_at_mine_n10_idx, atlas_at_mine_n11_idx,
                atlas_at_mine_n12_idx, atlas_at_mine_n13_idx, atlas_at_mine_n14_idx,
                atlas_at_mine_n15_idx};
            static const int kAtMineND[16] = {
                atlas_at_mined_15_idx,  atlas_at_mined_n1_idx,  atlas_at_mined_n2_idx,
                atlas_at_mined_n3_idx,  atlas_at_mined_n4_idx,  atlas_at_mined_n5_idx,
                atlas_at_mined_n6_idx,  atlas_at_mined_n7_idx,  atlas_at_mined_n8_idx,
                atlas_at_mined_n9_idx,  atlas_at_mined_n10_idx, atlas_at_mined_n11_idx,
                atlas_at_mined_n12_idx, atlas_at_mined_n13_idx, atlas_at_mined_n14_idx,
                atlas_at_mined_n15_idx};
            const int* kAtMineN = deep ? kAtMineND : kAtMineNL;
            int m = (openCell(tx, ty - 1) ? 1 : 0) | (openCell(tx + 1, ty) ? 2 : 0) |
                    (openCell(tx, ty + 1) ? 4 : 0) | (openCell(tx - 1, ty) ? 8 : 0);
            if (m == 15) {
                int nm = (!openCell(tx - 1, ty - 1) ? 1 : 0) | (!openCell(tx + 1, ty - 1) ? 2 : 0) |
                         (!openCell(tx - 1, ty + 1) ? 4 : 0) | (!openCell(tx + 1, ty + 1) ? 8 : 0);
                if (nm) {
                    renderer.drawSprite(kAtMineN[nm], sx, sy, 0.0f, eye, kSpriteScale);
                } else {
                    uint32_t h = visHash(tx * 7 + 3, ty * 13 + mineFloor_) % 100;
                    renderer.drawSprite(h < 70 ? kFloorVariants[0] : kFloorVariants[1 + h % 3],
                                        sx, sy, 0.0f, eye, kSpriteScale);
                }
            } else {
                renderer.drawSprite(kAtMine[m], sx, sy, 0.0f, eye, kSpriteScale);
            }
            if (k == 2) renderer.drawSprite(atlas_mine_hole_idx, sx, sy, 0.05f, eye, kSpriteScale);
            // Brick-room doorway: a dark passage through the 2-row south
            // wall, with the lantern-topped arch frame on the OUTER face
            // cell only (the arch pieces have transparent interiors).
            if (k == 7) {
                renderer.drawSprite(atlas_at_bwall_0_idx, sx, sy, 0.05f, eye, kSpriteScale);
                bool outer = ty + 1 >= kMineH || mine_[ty + 1][tx] != 7;
                if (outer) {
                    renderer.drawSprite(atlas_bface_arch2_idx, sx, sy, 0.06f, eye,
                                        kSpriteScale);
                }
            }
            // Abandoned-delving flavor (dungeon props sheet): the odd
            // crate, sack, or pot on open floor. Purely visual.
            if (k == 0) {
                uint32_t ph = visHash(tx * 3 + 1, ty * 5 + mineFloor_ * 17);
                if (ph % 43 == 0) {
                    // Tall props (crate/pot) skip spots directly north of
                    // a wall - their art would dangle over the wall band.
                    bool wallBelow = ty + 1 < kMineH &&
                                     (mine_[ty + 1][tx] == 1 || mine_[ty + 1][tx] == 6);
                    switch ((ph / 43) % 3) {
                        case 0:
                            if (!wallBelow) {
                                renderer.drawSprite(atlas_prop_crate_idx, sx - 16.0f,
                                                    sy - 32.0f, 0.06f, eye, kSpriteScale);
                            }
                            break;
                        case 1:
                            renderer.drawSprite(atlas_prop_sack_idx, sx, sy, 0.06f, eye,
                                                kSpriteScale);
                            break;
                        default:
                            if (!wallBelow) {
                                renderer.drawSprite(atlas_prop_pot_idx, sx, sy - 32.0f, 0.06f,
                                                    eye, kSpriteScale);
                            }
                            break;
                    }
                }
            }
            if (k == 4 || k == 5) {
                // Connection-aware track: straights along the run, quarter
                // turns at bends (the cart cell counts as track so rails
                // continue underneath it).
                auto railAt = [&](int32_t x, int32_t y) {
                    return x >= 0 && y >= 0 && x < kMineW && y < kMineH &&
                           (mine_[y][x] == 4 || mine_[y][x] == 5);
                };
                int rm = (railAt(tx, ty - 1) ? 1 : 0) | (railAt(tx + 1, ty) ? 2 : 0) |
                         (railAt(tx, ty + 1) ? 4 : 0) | (railAt(tx - 1, ty) ? 8 : 0);
                switch (rm) {
                    case 5:
                        renderer.drawSprite(atlas_rail_v_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 4: // run starts here, heading south
                        renderer.drawSprite(atlas_rail_v_t_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 1: // run ends here, coming from the north
                        renderer.drawSprite(atlas_rail_v_b_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 2: // run starts here, heading east
                        renderer.drawSprite(atlas_rail_h_l_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 8: // run ends here, coming from the west
                        renderer.drawSprite(atlas_rail_h_r_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 3:
                        renderer.drawSprite(atlas_rail_c_ne_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 9:
                        renderer.drawSprite(atlas_rail_c_nw_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 6:
                        renderer.drawSprite(atlas_rail_c_se_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 12:
                        renderer.drawSprite(atlas_rail_c_sw_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                    case 7: case 11: case 13: case 14: case 15:
                        // T/cross junction - no dedicated piece in the
                        // pack, so overlay both straights as a crossing.
                        renderer.drawSprite(atlas_rail_h_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        renderer.drawSprite(atlas_rail_v_idx, sx, sy, 0.06f, eye, kSpriteScale);
                        break;
                    default: // 0, 10: lone or horizontal mid
                        renderer.drawSprite(atlas_rail_h_idx, sx, sy, 0.05f, eye, kSpriteScale);
                        break;
                }
            }
        }
    }

    // Sorted entities: portal, cart, nodes, foes, player - and the walls'
    // walk-behind overlays (crowns, ridge strips, peeled rims), which sort
    // against the player like any other tall thing whose base is below it.
    struct Drawable {
        float anchorY;
        float x, y;
        int sprite;
        float depth;
        bool flip = false;
    };
    constexpr int kMaxDrawables = 320;
    Drawable items[kMaxDrawables];
    int count = 0;
    for (int32_t ty = tileMinY; ty <= tileMaxY; ty++) {
        for (int32_t tx = tileMinX; tx <= tileMaxX; tx++) {
            if (tx < 0 || ty < 0 || tx >= kMineW || ty >= kMineH) continue;
            if (count >= kMaxDrawables - 2) break;
            float sx = (static_cast<float>(tx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float sy = (static_cast<float>(ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
            float baseY = static_cast<float>(ty) + 1.0f;
            uint8_t k = mine_[ty][tx];
            if (k != 1 && k != 6) {
                // The overlay the wall SOUTH of this floor cell lifts onto
                // it (crown / ridge strip / peeled walk-behind rim). Its
                // anchor is that wall's BASE row - one below the wall - so a
                // player standing on this very cell sorts behind it. This is
                // the walk-behind: the wall top covers the player.
                const WallOpenCtx octx{&mine_[0][0], kMineW, kMineH};
                core::wallauto::Role ov =
                    core::wallauto::overlayRole(wallOpenAt, &octx, tx, ty);
                if (ov != core::wallauto::Role::None) {
                    bool brickBelow = ty + 1 < kMineH && mine_[ty + 1][tx] == 6;
                    items[count++] = {static_cast<float>(ty) + 2.0f, sx, sy,
                                      wallSpriteFor(ov, brickBelow), 0.02f};
                }
            }
            if (k == 3) {
                items[count++] = {baseY, sx - 16.0f, sy - 32.0f, atlas_place_mineshaft_idx, 0.6f};
            } else if (k == 5) {
                // Parked cart: side view on east-west track, front view
                // on north-south track (the run can bend now).
                bool ns = (ty > 0 && (mine_[ty - 1][tx] == 4 || mine_[ty - 1][tx] == 5)) ||
                          (ty < kMineH - 1 && (mine_[ty + 1][tx] == 4 || mine_[ty + 1][tx] == 5));
                items[count++] = {baseY, sx, sy - 16.0f, ns ? atlas_cart_ns_idx : atlas_cart_ew_idx,
                                  0.55f};
            } else if (k >= 10) {
                items[count++] = {baseY, sx, sy, kNodeSprites[k - 10], 0.55f};
            }
        }
    }
    for (const MineEnemy& e : foes_) {
        if (e.hurtT > 0.0f && (animFrame_ % 4) < 2) continue; // hit flash
        float cx = (e.x - camX) * kScreenTilePx + kTopScreenW / 2.0f;
        float feetY = (e.y - camY) * kScreenTilePx + kTopScreenH / 2.0f + 12.0f;
        if (count >= kMaxDrawables - 2) break;
        if (e.kind == 0) {
            int f = e.hopping ? 2 : (animFrame_ / 16) % 2;
            static const int kSlime[3] = {atlas_slime_0_idx, atlas_slime_1_idx, atlas_slime_2_idx};
            items[count++] = {e.y + 0.4f, cx - 16.0f, feetY - 32.0f, kSlime[f], 0.6f, e.vx < -0.05f};
        } else {
            float bob = std::sin(e.aiT * 6.0f) * 4.0f;
            int f = (animFrame_ / (e.kind == 2 ? 4 : 6)) % 2;
            int sprite = e.kind == 2 ? (f ? atlas_smallbat_1_idx : atlas_smallbat_0_idx)
                                     : (f ? atlas_bat_1_idx : atlas_bat_0_idx);
            items[count++] = {e.y + 0.4f, cx - 32.0f, feetY - 56.0f + bob, sprite, 0.7f};
        }
    }
    // Player (blinks while invulnerable).
    if (invulnT_ <= 0.0f || (animFrame_ % 6) < 3) {
        float cx = (minePos_.x - camX) * kScreenTilePx + kTopScreenW / 2.0f;
        float feetY = (minePos_.y - camY) * kScreenTilePx + kTopScreenH / 2.0f + 16.0f;
        bool acting = actionTimer_ > 0;
        float half = acting ? 32.0f : 24.0f;
        float height = acting ? 64.0f : 48.0f;
        items[count++] = {minePos_.y + 0.5f, cx - half, feetY - height, playerSprite(), 0.8f};
        if (riding_ && count < kMaxDrawables - 2) {
            bool ew = state_->facing == core::Facing::Left || state_->facing == core::Facing::Right;
            items[count++] = {minePos_.y + 0.505f, cx - 16.0f, feetY - 40.0f,
                              ew ? atlas_cart_ew_idx : atlas_cart_ns_idx, 0.82f};
        }
    }
    std::sort(items, items + count, [](const Drawable& a, const Drawable& b) {
        if (a.anchorY != b.anchorY) return a.anchorY < b.anchorY;
        if (a.x != b.x) return a.x < b.x;
        return a.sprite < b.sprite;
    });
    for (int i = 0; i < count; i++) {
        if (items[i].flip) {
            renderer.drawSpriteFlipped(items[i].sprite, items[i].x, items[i].y, items[i].depth,
                                       eye, kSpriteScale);
        } else {
            renderer.drawSprite(items[i].sprite, items[i].x, items[i].y, items[i].depth, eye,
                                kSpriteScale);
        }
    }

    // It's dark down here - a heavy tint, pushed back by your torchlight
    // and the exit lantern.
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, kTopScreenW, kTopScreenH, C2D_Color32(6, 4, 12, 120));
    float pcx = (minePos_.x - camX) * kScreenTilePx + kTopScreenW / 2.0f;
    float pcy = (minePos_.y - camY) * kScreenTilePx + kTopScreenH / 2.0f;
    C2D_DrawCircleSolid(pcx, pcy, 0.0f, 78.0f, C2D_Color32(255, 200, 110, 26));
    C2D_DrawCircleSolid(pcx, pcy, 0.0f, 44.0f, C2D_Color32(255, 214, 128, 30));
    for (int32_t ty = tileMinY; ty <= tileMaxY; ty++) {
        for (int32_t tx = tileMinX; tx <= tileMaxX; tx++) {
            if (tx < 0 || ty < 0 || tx >= kMineW || ty >= kMineH) continue;
            if (mine_[ty][tx] != 3) continue;
            float gx = (static_cast<float>(tx) + 0.5f - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float gy = (static_cast<float>(ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
            C2D_DrawCircleSolid(gx, gy, 0.0f, 40.0f, C2D_Color32(255, 190, 96, 34));
        }
    }

    // Contextual prompts, bottom-left.
    {
        char aLbl[24] = {0}, bLbl[24] = "Attack";
        if (riding_) {
            snprintf(aLbl, sizeof(aLbl), "Hop out");
            bLbl[0] = 0;
        } else {
            core::Vec2f off = facingOffset();
            int fx = static_cast<int>(std::floor(minePos_.x + off.x));
            int fy = static_cast<int>(std::floor(minePos_.y + off.y));
            int px = static_cast<int>(std::floor(minePos_.x));
            int py = static_cast<int>(std::floor(minePos_.y));
            uint8_t cell = (fx >= 0 && fy >= 0 && fx < kMineW && fy < kMineH) ? mine_[fy][fx] : 1;
            if (cell == 3) {
                snprintf(aLbl, sizeof(aLbl), mineFloor_ <= 1 ? "Leave" : "Climb up");
            } else if (cell == 2) {
                snprintf(aLbl, sizeof(aLbl), "Descend");
            } else if (cell >= 10) {
                snprintf(aLbl, sizeof(aLbl), "Mine");
            } else if (px >= 0 && py >= 0 && px < kMineW && py < kMineH &&
                       (mine_[py][px] == 4 || mine_[py][px] == 5)) {
                snprintf(aLbl, sizeof(aLbl), "Ride");
            }
        }
        drawPromptBar(renderer, eye, aLbl, bLbl);
    }

    // Hearts + floor depth, top-left.
    for (int i = 0; i < 3; i++) {
        int spr = hp_ >= (i + 1) * 2 ? atlas_heart_full_idx
                                     : (hp_ == i * 2 + 1 ? atlas_heart_half_idx : atlas_heart_empty_idx);
        renderer.drawSprite(spr, 8.0f + i * 34.0f, 8.0f, 0.95f, eye, kSpriteScale);
    }
    char depth[24];
    snprintf(depth, sizeof(depth), "Floor %d", mineFloor_);
    renderer.drawText(depth, 330.0f, 8.0f, 0.95f, eye, 0.55f, C2D_Color32(0xE8, 0xD8, 0xA0, 0xFF));
}

void WorldScene::drawWorld(const platform::Renderer& renderer, int eye) const {
    float camX = state_->playerPos.x;
    float camY = state_->playerPos.y;
    int64_t now = core::nowSeconds();

    int32_t tileMinX = static_cast<int32_t>(std::floor(camX)) - 7;
    int32_t tileMaxX = static_cast<int32_t>(std::floor(camX)) + 7;
    int32_t tileMinY = static_cast<int32_t>(std::floor(camY)) - 5;
    int32_t tileMaxY = static_cast<int32_t>(std::floor(camY)) + 5;

    static const int kGrassVariants[6] = {atlas_tile_grass_1_idx, atlas_tile_grass_2_idx,
                                          atlas_tile_grass_3_idx, atlas_tile_grass_4_idx,
                                          atlas_tile_grass_5_idx, atlas_tile_grass_6_idx};
    static const int kWaterFrames[4] = {atlas_tile_water_0_idx, atlas_tile_water_1_idx,
                                        atlas_tile_water_2_idx, atlas_tile_water_3_idx};

    static const int kSnowVariants[4] = {atlas_tile_snow_1_idx, atlas_tile_snow_2_idx,
                                         atlas_tile_snow_3_idx, atlas_tile_snow_4_idx};
    static const int kIceVariants[4] = {atlas_tile_ice_0_idx, atlas_tile_ice_1_idx,
                                        atlas_tile_ice_2_idx, atlas_tile_ice_3_idx};

    // Autotile sets - indexed by the 4-bit cardinal same-material mask
    // (N=1 E=2 S=4 W=8), sliced from the packs' standard blob tilesets by
    // prep_assets.py. This is what gives paths corners and turns, tilled
    // beds rounded edges, the snow line a real melt border, and grass a
    // proper overhanging shoreline against water and dug pits.
    static const int kAtGrass[16] = {
        atlas_at_grass_0_idx,  atlas_at_grass_1_idx,  atlas_at_grass_2_idx,  atlas_at_grass_3_idx,
        atlas_at_grass_4_idx,  atlas_at_grass_5_idx,  atlas_at_grass_6_idx,  atlas_at_grass_7_idx,
        atlas_at_grass_8_idx,  atlas_at_grass_9_idx,  atlas_at_grass_10_idx, atlas_at_grass_11_idx,
        atlas_at_grass_12_idx, atlas_at_grass_13_idx, atlas_at_grass_14_idx, atlas_at_grass_15_idx};
    static const int kAtSnow[16] = {
        atlas_at_snow_0_idx,  atlas_at_snow_1_idx,  atlas_at_snow_2_idx,  atlas_at_snow_3_idx,
        atlas_at_snow_4_idx,  atlas_at_snow_5_idx,  atlas_at_snow_6_idx,  atlas_at_snow_7_idx,
        atlas_at_snow_8_idx,  atlas_at_snow_9_idx,  atlas_at_snow_10_idx, atlas_at_snow_11_idx,
        atlas_at_snow_12_idx, atlas_at_snow_13_idx, atlas_at_snow_14_idx, atlas_at_snow_15_idx};
    static const int kAtPath[16] = {
        atlas_at_path_0_idx,  atlas_at_path_1_idx,  atlas_at_path_2_idx,  atlas_at_path_3_idx,
        atlas_at_path_4_idx,  atlas_at_path_5_idx,  atlas_at_path_6_idx,  atlas_at_path_7_idx,
        atlas_at_path_8_idx,  atlas_at_path_9_idx,  atlas_at_path_10_idx, atlas_at_path_11_idx,
        atlas_at_path_12_idx, atlas_at_path_13_idx, atlas_at_path_14_idx, atlas_at_path_15_idx};
    static const int kAtPStone[16] = {
        atlas_at_pstone_0_idx,  atlas_at_pstone_1_idx,  atlas_at_pstone_2_idx,
        atlas_at_pstone_3_idx,  atlas_at_pstone_4_idx,  atlas_at_pstone_5_idx,
        atlas_at_pstone_6_idx,  atlas_at_pstone_7_idx,  atlas_at_pstone_8_idx,
        atlas_at_pstone_9_idx,  atlas_at_pstone_10_idx, atlas_at_pstone_11_idx,
        atlas_at_pstone_12_idx, atlas_at_pstone_13_idx, atlas_at_pstone_14_idx,
        atlas_at_pstone_15_idx};
    static const int kAtTill[16] = {
        atlas_at_till_0_idx,  atlas_at_till_1_idx,  atlas_at_till_2_idx,  atlas_at_till_3_idx,
        atlas_at_till_4_idx,  atlas_at_till_5_idx,  atlas_at_till_6_idx,  atlas_at_till_7_idx,
        atlas_at_till_8_idx,  atlas_at_till_9_idx,  atlas_at_till_10_idx, atlas_at_till_11_idx,
        atlas_at_till_12_idx, atlas_at_till_13_idx, atlas_at_till_14_idx, atlas_at_till_15_idx};
    static const int kAtFloor[16] = {
        atlas_at_floor_0_idx,  atlas_at_floor_1_idx,  atlas_at_floor_2_idx,  atlas_at_floor_3_idx,
        atlas_at_floor_4_idx,  atlas_at_floor_5_idx,  atlas_at_floor_6_idx,  atlas_at_floor_7_idx,
        atlas_at_floor_8_idx,  atlas_at_floor_9_idx,  atlas_at_floor_10_idx, atlas_at_floor_11_idx,
        atlas_at_floor_12_idx, atlas_at_floor_13_idx, atlas_at_floor_14_idx, atlas_at_floor_15_idx};

    // Inner-corner ("notch") variants: interior tiles whose cardinals all
    // match but one or more DIAGONALS don't - crossings and concave
    // junctions - indexed by NW=1 NE=2 SW=4 SE=8 missing-diagonal bits
    // ([0] is the plain interior).
#define MYFARM_NOTCH_SET(mat)                                                                     \
    {atlas_##mat##_15_idx,  atlas_##mat##_n1_idx,  atlas_##mat##_n2_idx,  atlas_##mat##_n3_idx,   \
     atlas_##mat##_n4_idx,  atlas_##mat##_n5_idx,  atlas_##mat##_n6_idx,  atlas_##mat##_n7_idx,   \
     atlas_##mat##_n8_idx,  atlas_##mat##_n9_idx,  atlas_##mat##_n10_idx, atlas_##mat##_n11_idx,  \
     atlas_##mat##_n12_idx, atlas_##mat##_n13_idx, atlas_##mat##_n14_idx, atlas_##mat##_n15_idx}
    static const int kAtGrassN[16] = MYFARM_NOTCH_SET(at_grass);
    static const int kAtSnowN[16] = MYFARM_NOTCH_SET(at_snow);
    static const int kAtPathN[16] = MYFARM_NOTCH_SET(at_path);
    static const int kAtPStoneN[16] = MYFARM_NOTCH_SET(at_pstone);
    static const int kAtTillN[16] = MYFARM_NOTCH_SET(at_till);
    static const int kAtHillN[16] = MYFARM_NOTCH_SET(at_hill);
    static const int kAtHedgeN[16] = MYFARM_NOTCH_SET(at_hedge);
#undef MYFARM_NOTCH_SET
    // Hedge-maze walls (premium bush blob).
    static const int kAtHedge[16] = {
        atlas_at_hedge_0_idx,  atlas_at_hedge_1_idx,  atlas_at_hedge_2_idx,
        atlas_at_hedge_3_idx,  atlas_at_hedge_4_idx,  atlas_at_hedge_5_idx,
        atlas_at_hedge_6_idx,  atlas_at_hedge_7_idx,  atlas_at_hedge_8_idx,
        atlas_at_hedge_9_idx,  atlas_at_hedge_10_idx, atlas_at_hedge_11_idx,
        atlas_at_hedge_12_idx, atlas_at_hedge_13_idx, atlas_at_hedge_14_idx,
        atlas_at_hedge_15_idx};
    // Hill-plateau rims (premium hill tileset; interiors are plain grass
    // so only edge masks ever draw).
    static const int kAtHill[16] = {
        atlas_at_hill_0_idx,  atlas_at_hill_1_idx,  atlas_at_hill_2_idx,  atlas_at_hill_3_idx,
        atlas_at_hill_4_idx,  atlas_at_hill_5_idx,  atlas_at_hill_6_idx,  atlas_at_hill_7_idx,
        atlas_at_hill_8_idx,  atlas_at_hill_9_idx,  atlas_at_hill_10_idx, atlas_at_hill_11_idx,
        atlas_at_hill_12_idx, atlas_at_hill_13_idx, atlas_at_hill_14_idx, atlas_at_hill_15_idx};

    // Ground pass.
    for (int32_t ty = tileMinY; ty <= tileMaxY; ty++) {
        for (int32_t tx = tileMinX; tx <= tileMaxX; tx++) {
            const core::Tile& tile = state_->world.tileAt(tx, ty);
            float sx = (static_cast<float>(tx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float sy = (static_cast<float>(ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
            // Interior band: anything outside a stamped room is void.
            if (ty > kInteriorViewY && tile.terrain != core::Terrain::Floor) {
                C2D_DrawRectSolid(sx, sy, 0.0f, kScreenTilePx, kScreenTilePx,
                                  C2D_Color32(16, 13, 18, 255));
                continue;
            }
            bool snowHere = core::biomeAt(state_->worldSeed, tx, ty) == core::Biome::Snow;

            // Cardinal-neighbor mask for the blob sets above.
            auto maskOf = [&](auto&& pred) {
                int m = 0;
                if (pred(tx, ty - 1)) m |= 1;
                if (pred(tx + 1, ty)) m |= 2;
                if (pred(tx, ty + 1)) m |= 4;
                if (pred(tx - 1, ty)) m |= 8;
                return m;
            };
            // Missing-diagonal mask for interior tiles (mask == 15): which
            // corners need an inner-corner notch.
            auto notchOf = [&](auto&& pred) {
                int n = 0;
                if (!pred(tx - 1, ty - 1)) n |= 1;
                if (!pred(tx + 1, ty - 1)) n |= 2;
                if (!pred(tx - 1, ty + 1)) n |= 4;
                if (!pred(tx + 1, ty + 1)) n |= 8;
                return n;
            };
            // "Solid ground" - what grass/snow visually connect to. Only
            // water and dug pits break the turf.
            auto grassy = [&](int32_t x, int32_t y) {
                core::Terrain t = state_->world.tileAt(x, y).terrain;
                return t != core::Terrain::Water && t != core::Terrain::Hole;
            };
            // A blob edge's rounded corner exposes whatever is underneath:
            // water (ice up north) beside a shoreline, otherwise a pit.
            auto edgeBase = [&]() {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (state_->world.tileAt(tx + dx, ty + dy).terrain == core::Terrain::Water) {
                            return snowHere ? kIceVariants[visHash(tx, ty) % 4]
                                            : kWaterFrames[(animFrame_ / 16) % 4];
                        }
                    }
                }
                return atlas_tile_hole_idx;
            };
            uint32_t h = visHash(tx, ty) % 100;
            int flatBiome = snowHere ? (h < 72 ? atlas_tile_snow_0_idx : kSnowVariants[(h - 72) % 4])
                                     : (h < 72 ? atlas_tile_grass_0_idx : kGrassVariants[(h - 72) % 6]);

            if (tile.tilled) {
                auto tilledAt = [&](int32_t x, int32_t y) { return state_->world.tileAt(x, y).tilled; };
                int m = maskOf(tilledAt);
                int nm = m == 15 ? notchOf(tilledAt) : 0;
                if (m != 15 || nm) renderer.drawSprite(flatBiome, sx, sy, 0.0f, eye, kSpriteScale);
                // Watered soil = the SAME sprite, tinted darker at draw
                // time - a separate wet sprite set made hard seams
                // wherever wet met dry (or the field's own edge trim).
                int tillSpr = nm ? kAtTillN[nm] : kAtTill[m];
                if (tile.watered) {
                    renderer.drawSpriteTinted(tillSpr, sx, sy, 0.01f, eye, kSpriteScale,
                                              C2D_Color32(0x4a, 0x3e, 0x34, 0xFF), 0.4f);
                } else {
                    renderer.drawSprite(tillSpr, sx, sy, 0.01f, eye, kSpriteScale);
                }
            } else {
                switch (tile.terrain) {
                    case core::Terrain::Water:
                        // Snowlands water is frozen solid (and walkable).
                        renderer.drawSprite(snowHere ? kIceVariants[visHash(tx, ty) % 4]
                                                     : kWaterFrames[(animFrame_ / 16) % 4],
                                            sx, sy, 0.0f, eye, kSpriteScale);
                        break;
                    case core::Terrain::Dirt:
                        renderer.drawSprite(atlas_tile_dirt_idx, sx, sy, 0.0f, eye, kSpriteScale);
                        break;
                    case core::Terrain::Hole:
                        renderer.drawSprite(atlas_tile_hole_idx, sx, sy, 0.0f, eye, kSpriteScale);
                        break;
                    case core::Terrain::Floor: {
                        // Wooden decks autotile too (procedural blob - see
                        // prep_assets.py): rounded plank edges with a
                        // shadowed rim, ground showing beneath.
                        int m = maskOf([&](int32_t x, int32_t y) {
                            return state_->world.tileAt(x, y).terrain == core::Terrain::Floor;
                        });
                        if (m != 15) renderer.drawSprite(flatBiome, sx, sy, 0.0f, eye, kSpriteScale);
                        renderer.drawSprite(kAtFloor[m], sx, sy, 0.01f, eye, kSpriteScale);
                        break;
                    }
                    case core::Terrain::Path:
                    case core::Terrain::PathDirt:
                    case core::Terrain::PathPlank: {
                        // The whole path family connects for mask purposes
                        // (rails included) so different path types butt
                        // together without edge trims between them.
                        auto pathy = [&](int32_t x, int32_t y) {
                            return core::isPathTerrain(state_->world.tileAt(x, y).terrain);
                        };
                        int m = maskOf(pathy);
                        int nm = m == 15 ? notchOf(pathy) : 0;
                        if (m != 15 || nm) {
                            renderer.drawSprite(flatBiome, sx, sy, 0.0f, eye, kSpriteScale);
                        }
                        bool stone = tile.terrain == core::Terrain::Path;
                        renderer.drawSprite(nm ? (stone ? kAtPStoneN : kAtPathN)[nm]
                                               : (stone ? kAtPStone : kAtPath)[m],
                                            sx, sy, 0.01f, eye, kSpriteScale);
                        // Interior texture overlays: scatter stones on the
                        // stone path, laid planks on the plank path (kept
                        // off notched junction tiles to avoid overhang).
                        if (m == 15 && nm == 0 && stone) {
                            renderer.drawSprite(atlas_tile_path_idx, sx, sy, 0.02f, eye, kSpriteScale);
                        } else if (m == 15 && nm == 0 && tile.terrain == core::Terrain::PathPlank) {
                            renderer.drawSprite(atlas_path_planks_idx, sx, sy, 0.02f, eye, kSpriteScale);
                        }
                        break;
                    }
                    case core::Terrain::Rail: {
                        // Rails: a special kind of path - grass base with
                        // connection-aware track on top (same pieces as
                        // the Mine's cart tracks).
                        renderer.drawSprite(flatBiome, sx, sy, 0.0f, eye, kSpriteScale);
                        auto railAt = [&](int32_t x, int32_t y) {
                            return state_->world.tileAt(x, y).terrain == core::Terrain::Rail;
                        };
                        int rm = (railAt(tx, ty - 1) ? 1 : 0) | (railAt(tx + 1, ty) ? 2 : 0) |
                                 (railAt(tx, ty + 1) ? 4 : 0) | (railAt(tx - 1, ty) ? 8 : 0);
                        switch (rm) {
                            case 5:
                                renderer.drawSprite(atlas_rail_v_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 4:
                                renderer.drawSprite(atlas_rail_v_t_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 1:
                                renderer.drawSprite(atlas_rail_v_b_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 2:
                                renderer.drawSprite(atlas_rail_h_l_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 8:
                                renderer.drawSprite(atlas_rail_h_r_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 3:
                                renderer.drawSprite(atlas_rail_c_ne_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 9:
                                renderer.drawSprite(atlas_rail_c_nw_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 6:
                                renderer.drawSprite(atlas_rail_c_se_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 12:
                                renderer.drawSprite(atlas_rail_c_sw_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                            case 7: case 11: case 13: case 14: case 15:
                                renderer.drawSprite(atlas_rail_h_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                renderer.drawSprite(atlas_rail_v_idx, sx, sy, 0.03f, eye, kSpriteScale);
                                break;
                            default:
                                renderer.drawSprite(atlas_rail_h_idx, sx, sy, 0.02f, eye, kSpriteScale);
                                break;
                        }
                        break;
                    }
                    default: {
                        if (snowHere) {
                            // Snow connects to snow-biome ground only, so
                            // the biome boundary gets a real melt line.
                            auto snowy = [&](int32_t x, int32_t y) {
                                return grassy(x, y) &&
                                       core::biomeAt(state_->worldSeed, x, y) == core::Biome::Snow;
                            };
                            int m = maskOf(snowy);
                            int nm = m == 15 ? notchOf(snowy) : 0;
                            if (m == 15 && nm == 0) {
                                renderer.drawSprite(flatBiome, sx, sy, 0.0f, eye, kSpriteScale);
                            } else {
                                bool meadowNeighbor = maskOf([&](int32_t x, int32_t y) {
                                    return grassy(x, y) &&
                                           core::biomeAt(state_->worldSeed, x, y) != core::Biome::Snow;
                                }) != 0;
                                renderer.drawSprite(meadowNeighbor ? atlas_tile_grass_0_idx : edgeBase(),
                                                    sx, sy, 0.0f, eye, kSpriteScale);
                                renderer.drawSprite(nm ? kAtSnowN[nm] : kAtSnow[m], sx, sy, 0.01f, eye,
                                                    kSpriteScale);
                            }
                        } else {
                            int m = maskOf(grassy);
                            int nm = m == 15 ? notchOf(grassy) : 0;
                            if (m == 15 && nm == 0) {
                                renderer.drawSprite(flatBiome, sx, sy, 0.0f, eye, kSpriteScale);
                            } else {
                                renderer.drawSprite(edgeBase(), sx, sy, 0.0f, eye, kSpriteScale);
                                renderer.drawSprite(nm ? kAtGrassN[nm] : kAtGrass[m], sx, sy, 0.01f,
                                                    eye, kSpriteScale);
                            }
                        }
                        break;
                    }
                }
            }

            // Elevation: plateau rims and their stair ramps, layered over
            // whatever land terrain is up there. Interior plateau tiles
            // (mask 15, no notch) draw nothing - the top IS plain ground.
            if (tile.terrain != core::Terrain::Water && tile.terrain != core::Terrain::Floor &&
                core::elevAt(state_->worldSeed, tx, ty)) {
                auto raised = [&](int32_t x, int32_t y) {
                    return core::elevAt(state_->worldSeed, x, y);
                };
                int hm = maskOf(raised);
                if (hm != 15) {
                    // Stone-step ramp down a straight south-facing rim
                    // (mask 11 = only the south side is lower; the only
                    // place a cliff edge can be crossed - see
                    // cliffBlocked). The opening is a GAP in the ledge:
                    // the rim piece is skipped entirely so the stairs
                    // render on bare grass, and the art's jamb-rock arcs
                    // terminate the neighbors' lip instead.
                    if (hm == 11 && core::rampAt(state_->worldSeed, tx, ty)) {
                        renderer.drawSprite(atlas_hill_steps_idx, sx - 16.0f, sy, 0.04f, eye,
                                            kSpriteScale);
                    } else {
                        renderer.drawSprite(kAtHill[hm], sx, sy, 0.03f, eye, kSpriteScale);
                    }
                } else {
                    int hn = notchOf(raised);
                    if (hn) renderer.drawSprite(kAtHillN[hn], sx, sy, 0.03f, eye, kSpriteScale);
                }
            }

            // Hedge-maze walls: connection-drawn bush blob over the
            // grass, notches included so junctions read solid.
            if (tile.decoration == core::Decoration::Hedge) {
                auto hedgy = [&](int32_t x, int32_t y) {
                    return state_->world.tileAt(x, y).decoration == core::Decoration::Hedge;
                };
                int hgm = maskOf(hedgy);
                if (hgm == 15) {
                    int hgn = notchOf(hedgy);
                    renderer.drawSprite(hgn ? kAtHedgeN[hgn] : kAtHedge[15], sx, sy, 0.3f, eye,
                                        kSpriteScale);
                } else {
                    renderer.drawSprite(kAtHedge[hgm], sx, sy, 0.3f, eye, kSpriteScale);
                }
            }

            // Flat overlays that live at ground level. Open water gets a
            // hash-scattered mix of pads, rocks, and reeds (premium Water
            // Objects sheet) so big ponds aren't a flat blue field.
            if (tile.terrain == core::Terrain::Water && !snowHere &&
                tile.placed != core::Placed::Bridge) {
                uint32_t wh = visHash(tx, ty);
                static const int kWaterDecor[6] = {atlas_prop_lily_idx, atlas_wlily_0_idx,
                                                   atlas_wlily_1_idx,   atlas_wrock_0_idx,
                                                   atlas_wrock_1_idx,   atlas_wreed_0_idx};
                if (wh % 13 == 0) {
                    renderer.drawSprite(kWaterDecor[(wh / 13) % 6], sx, sy, 0.05f, eye,
                                        kSpriteScale);
                }
            }
            if (tile.placed == core::Placed::Bridge) {
                renderer.drawSprite(atlas_tile_bridge_idx, sx, sy, 0.05f, eye, kSpriteScale);
            } else if (tile.placed == core::Placed::Rug) {
                static const int kRugS[3] = {atlas_rug_s_0_idx, atlas_rug_s_1_idx, atlas_rug_s_2_idx};
                renderer.drawSprite(kRugS[tile.decoTier % 3], sx, sy, 0.05f, eye, kSpriteScale);
            } else if (tile.placed == core::Placed::RugLong) {
                static const int kRugL[3] = {atlas_rug_l_0_idx, atlas_rug_l_1_idx, atlas_rug_l_2_idx};
                renderer.drawSprite(kRugL[tile.decoTier % 3], sx, sy, 0.05f, eye, kSpriteScale);
            }
        }
    }

    // Sorted entity pass: crops, nodes, buildings, animals, the player -
    // back-to-front by the world Y of each thing's visual base, so the
    // player walks behind tall things whose base is below them.
    struct Drawable {
        float anchorY;
        float x, y;
        int sprite;
        float depth;
        bool flip = false;  // horizontally mirrored (creatures facing left)
        uint32_t tint = 0;  // nonzero = drawSpriteTinted (the crystal clone)
    };
    Drawable items[280];
    int count = 0;

    // Tier 2 trees are the premium fruit trees, hash-picked per tile so an
    // orchard-y mix of apple/orange/pear/peach appears in the far bands.
    static const int kFruitTrees[4] = {atlas_prop_tree_fruit_0_idx, atlas_prop_tree_fruit_1_idx,
                                       atlas_prop_tree_fruit_2_idx, atlas_prop_tree_fruit_3_idx};
    // Premium fence set, indexed by the same 4-bit connection mask as the
    // ground blobs - but as a LINE tileset it has true junctions: corners,
    // T-pieces, and a 4-way cross.
    static const int kAtFence[16] = {
        atlas_at_fence_0_idx,  atlas_at_fence_1_idx,  atlas_at_fence_2_idx,  atlas_at_fence_3_idx,
        atlas_at_fence_4_idx,  atlas_at_fence_5_idx,  atlas_at_fence_6_idx,  atlas_at_fence_7_idx,
        atlas_at_fence_8_idx,  atlas_at_fence_9_idx,  atlas_at_fence_10_idx, atlas_at_fence_11_idx,
        atlas_at_fence_12_idx, atlas_at_fence_13_idx, atlas_at_fence_14_idx, atlas_at_fence_15_idx};
    static const int kBushByTier[3] = {atlas_prop_bush_idx, atlas_prop_bush2_idx, atlas_prop_bush3_idx};
    static const int kMushByTier[3] = {atlas_prop_mushroom_idx, atlas_prop_mushroom2_idx, atlas_prop_mushroom3_idx};

    for (int32_t ty = tileMinY; ty <= tileMaxY; ty++) {
        for (int32_t tx = tileMinX; tx <= tileMaxX; tx++) {
            const core::Tile& tile = state_->world.tileAt(tx, ty);
            // Void tiles in the interior band draw nothing.
            if (ty > kInteriorViewY && tile.terrain != core::Terrain::Floor) continue;
            float sx = (static_cast<float>(tx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float sy = (static_cast<float>(ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
            float baseY = static_cast<float>(ty) + 1.0f;

            if (tile.hasCrop) {
                int stage = core::cropStageOf(tile, now);
                // Corn and sunflower late stages are 16x32 - a tile taller.
                bool tall = (tile.cropSpeciesId == core::kCropCorn ||
                             tile.cropSpeciesId == core::kCropSunflower) &&
                            stage >= 2;
                items[count++] = {baseY, sx, tall ? sy - 32.0f : sy,
                                  spriteForCropStage(tile.cropSpeciesId, stage, tile.watered),
                                  0.5f};
            } else if (tile.decoration == core::Decoration::None &&
                       tile.placed == core::Placed::None) {
                int bloom = bloomSpriteAt(tile, tx, ty, now);
                if (bloom >= 0) items[count++] = {baseY, sx, sy, bloom, 0.5f};
            }
            if (!tile.hasCrop && tile.decoration != core::Decoration::None) {
                bool ready = core::nodeReady(tile, now);
                int tier = clampTier(tile.decoTier);
                switch (tile.decoration) {
                    case core::Decoration::Tree: {
                        // Which biome's tree art this region uses (visual
                        // only - Meadow keeps the classic free-pack trees;
                        // birch/cherry/pine/snow come from the Sorry pack).
                        core::Biome biome = core::biomeAt(state_->worldSeed, tx, ty);
                        static const int kSlimByBiome[5] = {atlas_prop_tree_idx, atlas_tree_birch_idx,
                                                            atlas_tree_cherry_idx, atlas_tree_pine_idx,
                                                            atlas_tree_snow_idx};
                        static const int kBigByBiome[5] = {atlas_prop_tree_big_idx, atlas_tree_birch_big_idx,
                                                           atlas_tree_cherry_big_idx, atlas_tree_pine_big_idx,
                                                           atlas_tree_snow_big_idx};
                        static const int kStumpByBiome[5] = {atlas_prop_stump_idx, atlas_stump_birch_idx,
                                                             atlas_stump_cherry_idx, atlas_stump_pine_idx,
                                                             atlas_stump_pine_idx};
                        int b = static_cast<int>(biome);
                        bool snowBiome = biome == core::Biome::Snow;
                        if (!ready) {
                            // Tier 0 regrows as a sprout (also covers the
                            // player-planted sapling, which is tier 0);
                            // chopped big trees show a stump first, then a
                            // sprout for the back half of the regrow time.
                            bool sprouting =
                                tier == 0 ||
                                core::elapsedSeconds(tile.timestamp, now) * 2 >=
                                    core::kTreeBalance.respawnSec[tier];
                            if (sprouting) {
                                items[count++] = {baseY, sx, sy, atlas_prop_sapling_idx, 0.5f};
                            } else if (b == 0) {
                                // Meadow stump is 24x16-normalized.
                                items[count++] = {baseY, sx - 8.0f, sy, kStumpByBiome[0], 0.5f};
                            } else {
                                items[count++] = {baseY, sx, sy, kStumpByBiome[b], 0.5f};
                            }
                        } else if (tier == 0) {
                            items[count++] = {baseY, sx, sy - 32.0f, kSlimByBiome[b], 0.7f};
                        } else if (snowBiome) {
                            // Snowy big tree is 40x48-normalized - a full
                            // three tiles wide on screen (tiers 1+2).
                            items[count++] = {baseY, sx - 24.0f, sy - 64.0f, kBigByBiome[4], 0.7f};
                        } else if (tier == 1) {
                            items[count++] = {baseY, sx - 16.0f, sy - 32.0f, kBigByBiome[b], 0.7f};
                        } else {
                            items[count++] = {baseY, sx - 16.0f, sy - 32.0f,
                                              kFruitTrees[visHash(tx, ty) % 4], 0.7f};
                        }
                        break;
                    }
                    case core::Decoration::Rock:
                        if (!ready) {
                            items[count++] = {baseY, sx, sy, atlas_prop_pebble_idx, 0.6f};
                        } else if (tier == 0) {
                            items[count++] = {baseY, sx, sy, atlas_prop_rock_idx, 0.6f};
                        } else if (tier == 1) {
                            // 32x24-normalized boulder cluster.
                            items[count++] = {baseY, sx - 16.0f, sy - 16.0f, atlas_prop_rock2_idx, 0.6f};
                        } else {
                            // 40x40-normalized mossy boulder.
                            items[count++] = {baseY, sx - 24.0f, sy - 48.0f, atlas_prop_rock3_idx, 0.65f};
                        }
                        break;
                    case core::Decoration::Bush:
                        items[count++] = {baseY, sx, sy, ready ? kBushByTier[tier] : atlas_prop_bush_empty_idx, 0.6f};
                        break;
                    case core::Decoration::Mushroom:
                        if (!ready) {
                            items[count++] = {baseY, sx, sy, atlas_prop_sapling_idx, 0.6f};
                        } else if (tier == 2) {
                            // 32x16-normalized skull mushroom.
                            items[count++] = {baseY, sx - 16.0f, sy, kMushByTier[2], 0.6f};
                        } else {
                            items[count++] = {baseY, sx, sy, kMushByTier[tier], 0.6f};
                        }
                        break;
                    case core::Decoration::Pebble:
                        items[count++] = {baseY, sx, sy, atlas_prop_pebble_idx, 0.5f};
                        break;
                    case core::Decoration::WildPumpkin: {
                        // Wild plants show their own lifecycle: sprouts,
                        // half-growns, and ripe ones mixed through the
                        // patch (wildStageAt staggers them per tile).
                        static const int kWP[3] = {atlas_crop_pumpkin_1_idx,
                                                   atlas_crop_pumpkin_2_idx,
                                                   atlas_crop_pumpkin_3_idx};
                        items[count++] = {baseY, sx, sy, kWP[wildStageAt(tile, tx, ty, now) - 1],
                                          0.6f};
                        break;
                    }
                    case core::Decoration::WildSunflower: {
                        int ws = wildStageAt(tile, tx, ty, now);
                        if (ws == 1) {
                            items[count++] = {baseY, sx, sy, atlas_crop_sunflower_1_idx, 0.6f};
                        } else {
                            // Stages 2-3 are 16x32 - a tile taller.
                            items[count++] = {baseY, sx, sy - 32.0f,
                                              ws == 2 ? atlas_crop_sunflower_2_idx
                                                      : atlas_crop_sunflower_3_idx,
                                              0.6f};
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            switch (tile.placed) {
                case core::Placed::Fence: {
                    // Full 16-piece premium fence: rails also connect into
                    // gate leaves so a gate in a run keeps its rails.
                    auto fenceAt = [&](int32_t x, int32_t y) {
                        core::Placed p = state_->world.tileAt(x, y).placed;
                        return p == core::Placed::Fence || p == core::Placed::Gate ||
                               p == core::Placed::GateRight;
                    };
                    int m = (fenceAt(tx, ty - 1) ? 1 : 0) | (fenceAt(tx + 1, ty) ? 2 : 0) |
                            (fenceAt(tx, ty + 1) ? 4 : 0) | (fenceAt(tx - 1, ty) ? 8 : 0);
                    items[count++] = {baseY, sx, sy, kAtFence[m], 0.55f};
                    break;
                }
                case core::Placed::Camp: {
                    float ox, oy;
                    footprintOffsetForItem(core::kItemCamp, &ox, &oy);
                    items[count++] = {baseY, sx + ox, sy + oy, atlas_place_camp_idx, 0.7f};
                    break;
                }
                case core::Placed::Coop: {
                    float ox, oy;
                    footprintOffsetForItem(core::kItemCoop, &ox, &oy);
                    items[count++] = {baseY, sx + ox, sy + oy, atlas_place_coop_idx, 0.7f};
                    break;
                }
                case core::Placed::Barn: {
                    float ox, oy;
                    footprintOffsetForItem(core::kItemBarn, &ox, &oy);
                    items[count++] = {baseY, sx + ox, sy + oy, atlas_place_barn_idx, 0.7f};
                    break;
                }
                case core::Placed::Chest: {
                    // Chests take the wood of the biome they're placed in
                    // (Sorry-pack wood-typed chests): oak in the Meadow,
                    // birch/cherry/pine in their forests, pine up north.
                    // The crafted upgrade tiers override via decoTier
                    // (4 = silver, 5 = gold).
                    static const int kChestC[6] = {atlas_chest_oak_idx, atlas_chest_birch_idx,
                                                   atlas_chest_cherry_idx, atlas_chest_pine_idx,
                                                   atlas_chest_silver_idx, atlas_chest_gold_idx};
                    static const int kChestO[6] = {atlas_chest_oak_open_idx, atlas_chest_birch_open_idx,
                                                   atlas_chest_cherry_open_idx, atlas_chest_pine_open_idx,
                                                   atlas_chest_silver_open_idx, atlas_chest_gold_open_idx};
                    int wood;
                    if (tile.decoTier >= 4 && tile.decoTier <= 5) {
                        wood = tile.decoTier;
                    } else {
                        switch (core::biomeAt(state_->worldSeed, tx, ty)) {
                            case core::Biome::Birch: wood = 1; break;
                            case core::Biome::Cherry: wood = 2; break;
                            case core::Biome::Pine:
                            case core::Biome::Snow: wood = 3; break;
                            default: wood = 0; break;
                        }
                    }
                    float ox, oy;
                    footprintOffsetForItem(core::kItemChest, &ox, &oy);
                    bool open = chestOpen_ && chestX_ == tx && chestY_ == ty;
                    items[count++] = {baseY, sx + ox, sy + oy, open ? kChestO[wood] : kChestC[wood],
                                      0.6f};
                    break;
                }
                case core::Placed::Lamp: {
                    static const int kLamps[3] = {atlas_place_lamp_idx, atlas_lamp_1_idx,
                                                  atlas_lamp_2_idx};
                    items[count++] = {baseY, sx, sy, kLamps[tile.decoTier % 3], 0.6f};
                    break;
                }
                case core::Placed::Chair: {
                    // decoTier = facing (right/left/back/front), cycled
                    // by pressing A on the placed chair.
                    static const int kChairs[4] = {atlas_chair_0_idx, atlas_chair_1_idx,
                                                   atlas_chair_2_idx, atlas_chair_3_idx};
                    items[count++] = {baseY, sx, sy - 32.0f, kChairs[tile.decoTier % 4], 0.6f};
                    break;
                }
                case core::Placed::Bed: {
                    static const int kBeds[3] = {atlas_bed_0_idx, atlas_bed_1_idx, atlas_bed_2_idx};
                    items[count++] = {baseY, sx, sy - 32.0f, kBeds[tile.decoTier % 3], 0.6f};
                    break;
                }
                case core::Placed::Table: {
                    // decoTier = wood type (oak/birch/cherry/pine).
                    static const int kTables[4] = {atlas_table_w0_idx, atlas_table_w1_idx,
                                                   atlas_table_w2_idx, atlas_table_w3_idx};
                    items[count++] = {baseY, sx, sy - 32.0f, kTables[tile.decoTier % 4], 0.6f};
                    break;
                }
                case core::Placed::Dresser: {
                    static const int kDressers[4] = {atlas_dresser_w0_idx, atlas_dresser_w1_idx,
                                                     atlas_dresser_w2_idx, atlas_dresser_w3_idx};
                    items[count++] = {baseY, sx, sy - 32.0f, kDressers[tile.decoTier % 4], 0.6f};
                    break;
                }
                case core::Placed::Stool: {
                    static const int kStools[4] = {atlas_stool_w0_idx, atlas_stool_w1_idx,
                                                   atlas_stool_w2_idx, atlas_stool_w3_idx};
                    items[count++] = {baseY, sx, sy - 32.0f, kStools[tile.decoTier % 4], 0.6f};
                    break;
                }
                case core::Placed::Bench: {
                    static const int kBenches[4] = {atlas_bench_w0_idx, atlas_bench_w1_idx,
                                                    atlas_bench_w2_idx, atlas_bench_w3_idx};
                    items[count++] = {baseY, sx, sy - 32.0f, kBenches[tile.decoTier % 4], 0.6f};
                    break;
                }
                case core::Placed::Workbench:
                    items[count++] = {baseY, sx, sy - 32.0f, atlas_place_workbench_idx, 0.6f};
                    break;
                case core::Placed::Gate:
                case core::Placed::GateRight: {
                    // Gate leaves swing open when you're close. Each leaf
                    // is its own single-tile placeable now - pair a Gate
                    // (left) with a GateRight on the next tile for the
                    // classic double gate, or use one alone.
                    float gdx = state_->playerPos.x - (static_cast<float>(tx) + 0.5f);
                    float gdy = state_->playerPos.y - (static_cast<float>(ty) + 0.5f);
                    bool open = gdx * gdx + gdy * gdy < 1.8f;
                    int spr = tile.placed == core::Placed::Gate
                                  ? (open ? atlas_place_gate_l_open_idx : atlas_place_gate_l_idx)
                                  : (open ? atlas_place_gate_r_open_idx : atlas_place_gate_r_idx);
                    items[count++] = {baseY, sx, sy, spr, 0.55f};
                    break;
                }
                case core::Placed::Well:
                    items[count++] = {baseY, sx - 16.0f, sy - 32.0f, atlas_place_well_idx, 0.65f};
                    break;
                case core::Placed::Beehive: {
                    items[count++] = {baseY, sx - 16.0f, sy - 32.0f, atlas_place_beehive_idx, 0.65f};
                    // A worker bee circles every placed hive.
                    float t = static_cast<float>(animFrame_) * 0.06f + visHash(tx, ty) % 7;
                    float bx = sx + std::cos(t) * 22.0f;
                    float by = sy - 34.0f + std::sin(t * 1.7f) * 10.0f;
                    if (count < 279) {
                        items[count++] = {baseY + 0.01f, bx, by,
                                          (animFrame_ / 6) % 2 ? atlas_bee_1_idx : atlas_bee_0_idx, 0.85f};
                    }
                    break;
                }
                case core::Placed::Campfire: {
                    items[count++] = {baseY, sx, sy, atlas_place_campfire_idx, 0.55f};
                    static const int kFire[4] = {atlas_fire_0_idx, atlas_fire_1_idx,
                                                 atlas_fire_2_idx, atlas_fire_3_idx};
                    if (count < 279) {
                        items[count++] = {baseY + 0.01f, sx, sy - 14.0f,
                                          kFire[(animFrame_ / 8) % 4], 0.6f};
                    }
                    break;
                }
                case core::Placed::Sign:
                    items[count++] = {baseY, sx, sy, atlas_place_sign_idx, 0.55f};
                    break;
                case core::Placed::Mailbox:
                    items[count++] = {baseY, sx, sy - 32.0f, atlas_place_mailbox_idx, 0.6f};
                    break;
                case core::Placed::MineShaft:
                    items[count++] = {baseY, sx - 16.0f, sy - 32.0f, atlas_place_mineshaft_idx, 0.65f};
                    break;
                case core::Placed::Wall: {
                    if (ty > kInteriorViewY || tile.decoTier % 2 == 1) {
                        // Interior ring: the pack's 9-slice room kit -
                        // dark band north, thin frame strips east/west,
                        // light band south (corners built in). Free-
                        // standing dividers inside use the dark band.
                        auto voidAt = [&](int32_t x, int32_t y) {
                            return state_->world.tileAt(x, y).terrain != core::Terrain::Floor;
                        };
                        bool vN = voidAt(tx, ty - 1), vS = voidAt(tx, ty + 1);
                        bool vW = voidAt(tx - 1, ty), vE = voidAt(tx + 1, ty);
                        int spr = vS ? (vW ? atlas_iw_sw_idx
                                        : vE ? atlas_iw_se_idx
                                             : atlas_iw_s_idx)
                                  : vN ? (vW ? atlas_iw_nw_idx
                                          : vE ? atlas_iw_ne_idx
                                               : atlas_iw_n_idx)
                                       : (vW ? atlas_iw_w_idx
                                          : vE ? atlas_iw_e_idx
                                               : atlas_iw_n_idx);
                        items[count++] = {baseY, sx, sy, spr, 0.55f};
                        break;
                    }
                    // Horizontal runs get the framed trim columns at their
                    // ends; mids, lone walls, and vertical runs all use
                    // the plain panel piece. (The facade's center column
                    // is interior floor art, not wall - see prep_assets.)
                    auto wallAt = [&](int32_t x, int32_t y) {
                        core::Placed p = state_->world.tileAt(x, y).placed;
                        return p == core::Placed::Wall || p == core::Placed::Door;
                    };
                    bool w = wallAt(tx - 1, ty), e = wallAt(tx + 1, ty);
                    int spr = atlas_place_wall_idx;
                    if (e && !w) spr = atlas_wall_l_idx;
                    else if (w && !e) spr = atlas_wall_r_idx;
                    items[count++] = {baseY, sx, sy - 32.0f, spr, 0.6f};
                    break;
                }
                case core::Placed::Roof: {
                    // Roofs are 2D areas: the column (E/W neighbors) picks
                    // the left/mid/right trim, and the row position from
                    // the top picks the art - scalloped ridge for the top
                    // row, the band one row down, shingle fill deeper.
                    // Each 16x32 piece overhangs the tile above; southern
                    // rows Y-sort over northern ones, so the composite
                    // stacks into one continuous roof.
                    auto roofAt = [&](int32_t x, int32_t y) {
                        return state_->world.tileAt(x, y).placed == core::Placed::Roof;
                    };
                    static const int kRoofTop[4] = {atlas_rooftop_l_idx, atlas_rooftop_m_idx,
                                                    atlas_rooftop_m2_idx, atlas_rooftop_r_idx};
                    static const int kRoofBand[4] = {atlas_roof_l_idx, atlas_roof_m_idx,
                                                     atlas_roof_m2_idx, atlas_roof_r_idx};
                    static const int kRoofFill[4] = {atlas_rooffill_l_idx, atlas_rooffill_m_idx,
                                                     atlas_rooffill_m2_idx, atlas_rooffill_r_idx};
                    bool w = roofAt(tx - 1, ty), e = roofAt(tx + 1, ty);
                    int col = (w && e) ? ((tx & 1) ? 2 : 1) : (e ? 0 : (w ? 3 : 0));
                    const int* row = !roofAt(tx, ty - 1)       ? kRoofTop
                                     : !roofAt(tx, ty - 2)     ? kRoofBand
                                                               : kRoofFill;
                    items[count++] = {baseY, sx, sy - 32.0f, row[col], 0.6f};
                    break;
                }
                case core::Placed::Door: {
                    float ddx = state_->playerPos.x - (static_cast<float>(tx) + 0.5f);
                    float ddy = state_->playerPos.y - (static_cast<float>(ty) + 0.5f);
                    bool open = ddx * ddx + ddy * ddy < 1.8f;
                    if (ty > kInteriorViewY) {
                        // Interior exit: bare door leaf on the south band.
                        items[count++] = {baseY, sx, sy, atlas_iw_s_idx, 0.55f};
                        if (count < 279) {
                            items[count++] = {baseY + 0.004f, sx, sy,
                                              open ? atlas_door_leaf_open_idx
                                                   : atlas_door_leaf_idx,
                                              0.56f};
                        }
                        break;
                    }
                    items[count++] = {baseY, sx, sy - 32.0f,
                                      open ? atlas_place_door_open_idx : atlas_place_door_idx, 0.6f};
                    break;
                }
                case core::Placed::Cottage:
                    items[count++] = {baseY, sx - 48.0f, sy - 96.0f, atlas_place_cottage_idx, 0.7f};
                    break;
                case core::Placed::Hut:
                    items[count++] = {baseY, sx - 48.0f, sy - 96.0f, atlas_place_hut_idx, 0.7f};
                    break;
                case core::Placed::Manor:
                    // 96x80 source at 2x = 192x160, bottom-centered.
                    items[count++] = {baseY, sx - 80.0f, sy - 128.0f, atlas_place_manor_idx, 0.7f};
                    break;
                case core::Placed::Snowman:
                    items[count++] = {baseY, sx, sy - 32.0f, atlas_place_snowman_idx, 0.6f};
                    break;
                case core::Placed::Trough:
                    items[count++] = {baseY, sx, sy,
                                      tile.decoTier ? atlas_place_trough_full_idx
                                                    : atlas_place_trough_idx,
                                      0.6f};
                    break;
                case core::Placed::HayBale:
                    // decoTier: 0 = single bale, 1 = the long double bale
                    // (32px art spills onto the east tile).
                    items[count++] = {baseY, sx, sy,
                                      tile.decoTier ? atlas_place_haybale_l_idx
                                                    : atlas_place_haybale_idx,
                                      0.6f};
                    break;
                case core::Placed::WaterTray: {
                    // decoTier = fill level: 0 full, 1 half, 2 empty. A
                    // full watering can tops it up; it dries over time.
                    static const int kTray[3] = {atlas_watertray_0_idx, atlas_watertray_1_idx,
                                                 atlas_watertray_2_idx};
                    items[count++] = {baseY, sx, sy, kTray[tile.decoTier % 3], 0.6f};
                    break;
                }
                case core::Placed::Boat: {
                    // Moored rowboat, bobbing gently on the water.
                    float bob = std::sin(static_cast<float>(animFrame_) * 0.05f +
                                         static_cast<float>(visHash(tx, ty) % 7)) * 2.0f;
                    items[count++] = {baseY, sx - 32.0f, sy + bob, atlas_place_boat_idx, 0.6f};
                    break;
                }
                case core::Placed::Picnic:
                    items[count++] = {baseY, sx - 32.0f, sy - 32.0f, atlas_place_picnic_idx, 0.6f};
                    break;
                case core::Placed::Present: {
                    static const int kPresents[5] = {atlas_place_present_0_idx, atlas_place_present_1_idx,
                                                     atlas_place_present_2_idx, atlas_place_present_3_idx,
                                                     atlas_place_present_4_idx};
                    items[count++] = {baseY, sx, sy, kPresents[tile.decoTier % 5], 0.6f};
                    break;
                }
                case core::Placed::XmasTree:
                    items[count++] = {baseY, sx - 32.0f, sy - 96.0f,
                                      (animFrame_ / 16) % 2 ? atlas_place_xmas_1_idx
                                                            : atlas_place_xmas_0_idx,
                                      0.7f};
                    break;
                default:
                    break;
            }
        }
    }

    // Tamed animals idle around their building; a product icon floats up
    // when there's something to collect.
    static const float kHomeOffsets[4][2] = {{1.3f, 0.6f}, {-1.2f, 0.9f}, {0.4f, 1.4f}, {-0.5f, 1.7f}};
    for (size_t ai = 0; ai < state_->animals.size(); ai++) {
        const core::TamedAnimal& a = state_->animals[ai];
        // Slot around the building by how many earlier list entries share
        // this home - stable regardless of taming order.
        int perHome = 0;
        for (size_t j = 0; j < ai; j++) {
            if (state_->animals[j].homeX == a.homeX && state_->animals[j].homeY == a.homeY) perHome++;
        }
        const float* off = kHomeOffsets[perHome % 4];
        float wx = static_cast<float>(a.homeX) + 0.5f + off[0];
        float wy = static_cast<float>(a.homeY) + 1.0f + off[1];
        // Inside their coop/barn, the flock idles around the room instead
        // of around the building (same offsets, remapped to the room's
        // center - so walking in shows everyone at home).
        if (inInterior()) {
            core::InteriorData* room = state_->interiorAt(a.homeX, a.homeY);
            if (!room) continue;
            int32_t ax, ay;
            interiorAnchor(a.homeX, a.homeY, &ax, &ay);
            wx = static_cast<float>(ax) + 0.5f + off[0];
            wy = static_cast<float>(ay + room->h / 2) + off[1];
        }
        if (wx < tileMinX || wx > tileMaxX + 1 || wy < tileMinY || wy > tileMaxY + 1) continue;

        // Color-variant sprite tables: [variant][frame]. Variant 0 is the
        // classic free-pack art; 1-4 are the premium recolors. Babies use
        // the premium chick/calf sheets (all five colors).
        static const int kChickIdle[5][2] = {
            {atlas_chicken_0_idx, atlas_chicken_1_idx}, {atlas_chicken1_0_idx, atlas_chicken1_1_idx},
            {atlas_chicken2_0_idx, atlas_chicken2_1_idx}, {atlas_chicken3_0_idx, atlas_chicken3_1_idx},
            {atlas_chicken4_0_idx, atlas_chicken4_1_idx}};
        static const int kCowIdle[5][2] = {
            {atlas_cow_0_idx, atlas_cow_1_idx}, {atlas_cow1_0_idx, atlas_cow1_1_idx},
            {atlas_cow2_0_idx, atlas_cow2_1_idx}, {atlas_cow3_0_idx, atlas_cow3_1_idx},
            {atlas_cow4_0_idx, atlas_cow4_1_idx}};
        static const int kChickBaby[5][2] = {
            {atlas_chick0_0_idx, atlas_chick0_1_idx}, {atlas_chick1_0_idx, atlas_chick1_1_idx},
            {atlas_chick2_0_idx, atlas_chick2_1_idx}, {atlas_chick3_0_idx, atlas_chick3_1_idx},
            {atlas_chick4_0_idx, atlas_chick4_1_idx}};
        static const int kCowBaby[5][2] = {
            {atlas_calf0_0_idx, atlas_calf0_1_idx}, {atlas_calf1_0_idx, atlas_calf1_1_idx},
            {atlas_calf2_0_idx, atlas_calf2_1_idx}, {atlas_calf3_0_idx, atlas_calf3_1_idx},
            {atlas_calf4_0_idx, atlas_calf4_1_idx}};

        float cx = (wx - camX) * kScreenTilePx + kTopScreenW / 2.0f;
        float feetY = (wy - camY) * kScreenTilePx + kTopScreenH / 2.0f;
        bool chicken = a.species == core::AnimalSpecies::Chicken;
        bool grown = core::elapsedAtLeast(a.tamedAt, now, core::kBabyGrowSec);
        int v = a.variant % 5;
        int frame = (animFrame_ / 24) % 2;
        if (count < 278) {
            bool faceL = (perHome % 2) == 1; // alternate facing around the pen
            if (chicken) {
                // The first minute after taming, the chick is still an egg
                // (it wobbles, then hatches into the baby).
                bool egg = core::elapsedSeconds(a.tamedAt, now) < 60;
                const int* set = grown ? kChickIdle[v] : kChickBaby[v];
                int sprite = egg ? ((animFrame_ / 20) % 2 ? atlas_egg_1_idx : atlas_egg_0_idx)
                                 : set[frame];
                items[count++] = {wy, cx - 16.0f, feetY - 32.0f, sprite, 0.65f, !egg && faceL};
            } else {
                const int* set = grown ? kCowIdle[v] : kCowBaby[v];
                items[count++] = {wy, cx - 32.0f, feetY - 64.0f, set[frame], 0.65f, faceL};
            }
            int32_t interval = chicken ? core::kEggIntervalSec : core::kMilkIntervalSec;
            if (grown && core::elapsedAtLeast(a.lastCollectedAt, now, interval)) {
                float bob = std::sin(static_cast<float>(animFrame_) / 12.0f) * 2.0f;
                items[count++] = {wy + 0.01f, cx - 16.0f,
                                  feetY - (chicken ? 64.0f : 96.0f) + bob,
                                  chicken ? atlas_item_egg_idx : atlas_item_milk_idx, 0.9f};
            }
        }
    }

    // Dropped item stacks sitting on the ground.
    for (const core::GroundItem& g : state_->groundItems) {
        if (g.x < tileMinX || g.x > tileMaxX || g.y < tileMinY || g.y > tileMaxY) continue;
        if (count >= 279) break;
        float sx = (static_cast<float>(g.x) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
        float sy = (static_cast<float>(g.y) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
        items[count++] = {static_cast<float>(g.y) + 1.0f, sx, sy, spriteForItem(g.item), 0.52f};
    }

    // Wild animals (chickens/cows in five colors, plus frogs near water).
    {
        static const int kChickWild[5][4] = {
            // idle0, idle1, walk0, walk1 per variant
            {atlas_chicken_0_idx, atlas_chicken_1_idx, atlas_chicken_w0_idx, atlas_chicken_w1_idx},
            {atlas_chicken1_0_idx, atlas_chicken1_1_idx, atlas_chicken1_w0_idx, atlas_chicken1_w1_idx},
            {atlas_chicken2_0_idx, atlas_chicken2_1_idx, atlas_chicken2_w0_idx, atlas_chicken2_w1_idx},
            {atlas_chicken3_0_idx, atlas_chicken3_1_idx, atlas_chicken3_w0_idx, atlas_chicken3_w1_idx},
            {atlas_chicken4_0_idx, atlas_chicken4_1_idx, atlas_chicken4_w0_idx, atlas_chicken4_w1_idx}};
        static const int kCowWild[5][4] = {
            {atlas_cow_0_idx, atlas_cow_1_idx, atlas_cow_w0_idx, atlas_cow_w1_idx},
            {atlas_cow1_0_idx, atlas_cow1_1_idx, atlas_cow1_w0_idx, atlas_cow1_w1_idx},
            {atlas_cow2_0_idx, atlas_cow2_1_idx, atlas_cow2_w0_idx, atlas_cow2_w1_idx},
            {atlas_cow3_0_idx, atlas_cow3_1_idx, atlas_cow3_w0_idx, atlas_cow3_w1_idx},
            {atlas_cow4_0_idx, atlas_cow4_1_idx, atlas_cow4_w0_idx, atlas_cow4_w1_idx}};

        for (const WildAnimal& w : wild_) {
            if (w.x < tileMinX || w.x > tileMaxX + 1 || w.y < tileMinY || w.y > tileMaxY + 1) continue;
            float cx = (w.x - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float feetY = (w.y - camY) * kScreenTilePx + kTopScreenH / 2.0f;
            bool movingNow = std::fabs(w.tx - w.x) + std::fabs(w.ty - w.y) > 0.05f;
            int fastFrame = (animFrame_ / 8) % 2;
            int slowFrame = (animFrame_ / 24) % 2;
            if (count >= 279) break;
            int v = w.variant % 5;
            // The walk art faces right; mirror it when heading left so
            // nobody moonwalks (idle frames are symmetric enough that the
            // flip carries through harmlessly).
            if (w.kind == 3) { // ambient fish, half-submerged in the pond
                int f = movingNow ? fastFrame : slowFrame;
                int sprite = (w.variant % 2) ? (f ? atlas_fishamb_s1_idx : atlas_fishamb_s0_idx)
                                             : (f ? atlas_fishamb_1_idx : atlas_fishamb_0_idx);
                items[count++] = {w.y, cx - 16.0f, feetY - 24.0f, sprite, 0.55f, w.faceLeft};
            } else if (w.kind == 2) { // frogs come in two colors (variant parity)
                int f = movingNow ? fastFrame : slowFrame;
                int sprite = (w.variant % 2) ? (f ? atlas_frog2_1_idx : atlas_frog2_0_idx)
                                             : (f ? atlas_frog_1_idx : atlas_frog_0_idx);
                items[count++] = {w.y, cx - 16.0f, feetY - 32.0f, sprite, 0.65f, w.faceLeft};
            } else if (w.kind == 0) {
                int sprite = movingNow ? kChickWild[v][2 + fastFrame] : kChickWild[v][slowFrame];
                items[count++] = {w.y, cx - 16.0f, feetY - 32.0f, sprite, 0.65f, w.faceLeft};
            } else {
                int sprite = movingNow ? kCowWild[v][2 + fastFrame] : kCowWild[v][slowFrame];
                items[count++] = {w.y, cx - 32.0f, feetY - 64.0f, sprite, 0.65f, w.faceLeft};
            }
            // Tame-refusal speech bubble: a quick droplet pop, then the
            // small bubble with the food this animal wants inside (the cow
            // cycles through its menu). The tail tip rests just above the
            // head; bobs on the same clock as the produce-ready icons.
            if (w.reqT > 0 && w.kind <= 1 && count < 278) {
                float bob = std::sin(static_cast<float>(animFrame_) / 12.0f) * 2.0f;
                float headTop = feetY - (w.kind == 0 ? 32.0f : 64.0f);
                if (w.reqT > kReqBubbleFrames - kReqBubblePopFrames) {
                    items[count++] = {w.y + 0.01f, cx - 18.0f, headTop - 46.0f + bob,
                                      atlas_req_bubble_0_idx, 0.9f};
                } else {
                    items[count++] = {w.y + 0.01f, cx - 31.0f, headTop - 66.0f + bob,
                                      atlas_req_bubble_1_idx, 0.9f};
                    int icon = spriteForItem(core::kItemBerries);
                    if (w.kind == 1) {
                        static const core::ItemId kCowMenu[3] = {core::kItemHay, core::kItemTurnip,
                                                                 core::kItemApple};
                        icon = spriteForItem(
                            kCowMenu[((kReqBubbleFrames - w.reqT) / kReqBubbleCycleFrames) % 3]);
                    }
                    items[count++] = {w.y + 0.011f, cx - 16.0f, headTop - 56.0f + bob, icon, 0.92f};
                }
            }
        }
    }

    // Player (24x24-normalized frames; action poses are 32x32).
    {
        float cx = (state_->playerPos.x - camX) * kScreenTilePx + kTopScreenW / 2.0f;
        float feetY = (state_->playerPos.y - camY) * kScreenTilePx + kTopScreenH / 2.0f + 16.0f;
        bool acting = actionTimer_ > 0 && !swimming_;
        float half = acting ? 32.0f : 24.0f;
        float height = acting ? 64.0f : 48.0f;
        items[count++] = {state_->playerPos.y + 0.5f, cx - half, feetY - height, playerSprite(), 0.8f};
        // Riding: the minecart draws just in front of the player so it
        // covers their legs - side view on E/W track, back view on N/S.
        if (riding_ && count < 279) {
            bool ew = state_->facing == core::Facing::Left || state_->facing == core::Facing::Right;
            items[count++] = {state_->playerPos.y + 0.505f, cx - 16.0f, feetY - 40.0f,
                              ew ? atlas_cart_ew_idx : atlas_cart_ns_idx, 0.82f};
        }
        // Emote bubble floating over the player's head (plus the skill
        // icon on level-ups).
        if (emoteT_ > 0 && count < 279) {
            float bob = std::sin(static_cast<float>(animFrame_) / 9.0f) * 2.0f;
            if (emoteSprite_ == atlas_emote_cheer_idx && count < 277) {
                // Level-up cheers get a real speech bubble (tail pointing
                // at the player's head); the widened variant fits the
                // skill's signature icon beside the cheer.
                bool wide = emoteExtra_ >= 0;
                items[count++] = {state_->playerPos.y + 0.509f, cx - (wide ? 38.0f : 21.0f),
                                  feetY - height - 46.0f + bob,
                                  wide ? atlas_emote_bubble_wide_idx : atlas_emote_bubble_idx,
                                  1.0f};
                items[count++] = {state_->playerPos.y + 0.51f, cx - (wide ? 33.0f : 15.0f),
                                  feetY - height - 41.0f + bob, emoteSprite_, 0.92f};
                if (wide) {
                    items[count++] = {state_->playerPos.y + 0.512f, cx + 3.0f,
                                      feetY - height - 41.0f + bob, emoteExtra_, 0.93f};
                }
            } else {
                items[count++] = {state_->playerPos.y + 0.51f, cx - 16.0f,
                                  feetY - height - 34.0f + bob, emoteSprite_, 0.92f};
                if (emoteExtra_ >= 0 && count < 279) {
                    items[count++] = {state_->playerPos.y + 0.512f, cx + 18.0f,
                                      feetY - height - 30.0f + bob, emoteExtra_, 0.93f};
                }
            }
        }
    }

    // The crystal clone: your own walk frames with a gem-blue tint, plus
    // a bobbing tool icon showing its current orders.
    if (state_->clone.exists && count < 278) {
        const core::CloneData& cl = state_->clone;
        if (cl.pos.x > tileMinX && cl.pos.x < tileMaxX + 1 && cl.pos.y > tileMinY &&
            cl.pos.y < tileMaxY + 1) {
            static const int kCloneWalk[4][4] = {
                {atlas_player_down_0_idx, atlas_player_down_1_idx, atlas_player_down_2_idx,
                 atlas_player_down_3_idx},
                {atlas_player_up_0_idx, atlas_player_up_1_idx, atlas_player_up_2_idx,
                 atlas_player_up_3_idx},
                {atlas_player_left_0_idx, atlas_player_left_1_idx, atlas_player_left_2_idx,
                 atlas_player_left_3_idx},
                {atlas_player_right_0_idx, atlas_player_right_1_idx, atlas_player_right_2_idx,
                 atlas_player_right_3_idx},
            };
            int spr = cloneMoving_ ? kCloneWalk[cloneDir_][2 + (animFrame_ / 8) % 2]
                                   : kCloneWalk[cloneDir_][(animFrame_ / 32) % 2];
            float ccx = (cl.pos.x - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float cFeetY = (cl.pos.y - camY) * kScreenTilePx + kTopScreenH / 2.0f + 16.0f;
            items[count++] = {cl.pos.y + 0.5f, ccx - 24.0f, cFeetY - 48.0f, spr, 0.8f, false,
                              C2D_Color32(140, 220, 255, 255)};
            static const int kTaskIcons[5] = {-1, atlas_tool_axe_idx, atlas_tool_pickaxe_idx,
                                              atlas_item_berries_idx, atlas_crop_wheat_seed_idx};
            if (cl.task != 0 && count < 279) {
                float ibob = std::sin(static_cast<float>(animFrame_) / 10.0f) * 2.0f;
                items[count++] = {cl.pos.y + 0.51f, ccx - 16.0f, cFeetY - 84.0f + ibob,
                                  kTaskIcons[cl.task], 0.9f};
            }
        }
    }

    // Tree-chop leaf-poofs (13-frame premium fall animation; 64x48 art
    // whose standing tree's trunk base sits at (40,44) in frame space,
    // so the frame lands trunk-on-tile).
    {
        static const int kPoof[13] = {
            atlas_treefall_0_idx,  atlas_treefall_1_idx,  atlas_treefall_2_idx,
            atlas_treefall_3_idx,  atlas_treefall_4_idx,  atlas_treefall_5_idx,
            atlas_treefall_6_idx,  atlas_treefall_7_idx,  atlas_treefall_8_idx,
            atlas_treefall_9_idx,  atlas_treefall_10_idx, atlas_treefall_11_idx,
            atlas_treefall_12_idx};
        for (const TreePoof& p : poofs_) {
            int f = static_cast<int>(p.t / 0.06f);
            if (f > 12 || count >= 279) continue;
            float psx = (static_cast<float>(p.tx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
            float psy = (static_cast<float>(p.ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
            items[count++] = {static_cast<float>(p.ty) + 1.01f, psx - 64.0f, psy - 56.0f,
                              kPoof[f], 0.75f};
        }
    }

    // Fishing bobber on the faced water tile: gentle bobbing while the
    // line is out, a churning splash during the bite window.
    if (fishState_ != 0 && count < 279) {
        core::Vec2f foff = facingOffset();
        int32_t bx = static_cast<int32_t>(std::floor(state_->playerPos.x + foff.x));
        int32_t by = static_cast<int32_t>(std::floor(state_->playerPos.y + foff.y));
        float bsx = (static_cast<float>(bx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
        float bsy = (static_cast<float>(by) - camY) * kScreenTilePx + kTopScreenH / 2.0f;
        static const int kBob[4] = {atlas_bob_0_idx, atlas_bob_1_idx, atlas_bob_2_idx,
                                    atlas_bob_3_idx};
        int spr = fishState_ == 2 ? ((animFrame_ / 6) % 2 ? atlas_bite_1_idx : atlas_bite_0_idx)
                                  : kBob[(animFrame_ / 10) % 4];
        // 16x32 stack (bobber cell over splash cell) - the lower half
        // lands on the faced water tile.
        items[count++] = {static_cast<float>(by) + 1.02f, bsx, bsy - 32.0f, spr, 0.9f};
    }

    // Tie-break equal anchors by X (then sprite) so entities in the same
    // row keep a FIXED draw order no matter how the visible window shifts
    // - std::sort on equal keys otherwise reshuffles overlapping trees as
    // the camera moves.
    std::sort(items, items + count, [](const Drawable& a, const Drawable& b) {
        if (a.anchorY != b.anchorY) return a.anchorY < b.anchorY;
        if (a.x != b.x) return a.x < b.x;
        return a.sprite < b.sprite;
    });

    for (int i = 0; i < count; i++) {
        if (items[i].tint) {
            renderer.drawSpriteTinted(items[i].sprite, items[i].x, items[i].y, items[i].depth,
                                      eye, kSpriteScale, items[i].tint, 0.35f);
        } else if (items[i].flip) {
            renderer.drawSpriteFlipped(items[i].sprite, items[i].x, items[i].y, items[i].depth,
                                       eye, kSpriteScale);
        } else {
            renderer.drawSprite(items[i].sprite, items[i].x, items[i].y, items[i].depth, eye,
                                kSpriteScale);
        }
    }

    // --- Atmosphere: night tint, warm light glows, weather ------------------
    float dark = core::darkness(now);
    if (dark > 0.01f) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, kTopScreenW, kTopScreenH,
                          C2D_Color32(10, 14, 38, static_cast<uint8_t>(dark * 150.0f)));
        // Lamps, campfires, and Christmas trees push back the dark.
        for (int32_t ty = tileMinY; ty <= tileMaxY; ty++) {
            for (int32_t tx = tileMinX; tx <= tileMaxX; tx++) {
                const core::Tile& tile = state_->world.tileAt(tx, ty);
                if (tile.placed != core::Placed::Lamp && tile.placed != core::Placed::Campfire &&
                    tile.placed != core::Placed::XmasTree) {
                    continue;
                }
                float gx = (static_cast<float>(tx) + 0.5f - camX) * kScreenTilePx + kTopScreenW / 2.0f;
                float gy = (static_cast<float>(ty) + 0.5f - camY) * kScreenTilePx + kTopScreenH / 2.0f;
                float flicker = tile.placed == core::Placed::Campfire
                                    ? 3.0f * std::sin(static_cast<float>(animFrame_) * 0.31f)
                                    : 0.0f;
                uint8_t a1 = static_cast<uint8_t>(dark * 42.0f);
                uint8_t a2 = static_cast<uint8_t>(dark * 60.0f);
                C2D_DrawCircleSolid(gx, gy - 16.0f, 0.0f, 46.0f + flicker, C2D_Color32(255, 190, 96, a1));
                C2D_DrawCircleSolid(gx, gy - 16.0f, 0.0f, 26.0f + flicker, C2D_Color32(255, 214, 128, a2));
            }
        }
    }

    if (!inInterior() && core::weatherAt(state_->worldSeed, now) == core::Weather::Rain) {
        bool snowfall = core::biomeAt(state_->worldSeed,
                                      static_cast<int32_t>(std::floor(camX)),
                                      static_cast<int32_t>(std::floor(camY))) == core::Biome::Snow;
        for (int i = 0; i < 44; i++) {
            uint32_t h = visHash(i * 31 + 7, 991);
            float fall = snowfall ? 0.9f : 5.5f;
            float x0 = static_cast<float>(h % 400);
            float y = std::fmod(static_cast<float>(h / 7 % 240) +
                                    static_cast<float>(animFrame_) * fall,
                                260.0f) -
                      10.0f;
            float x = x0 + (snowfall
                                ? std::sin((static_cast<float>(animFrame_) + h) * 0.04f) * 7.0f
                                : -y * 0.05f);
            x += platform::stereoShift(0.9f, eye);
            if (snowfall) {
                // Real snowflake sprites (winter pack), three shapes.
                static const int kFlakes[3] = {atlas_snowflake_0_idx, atlas_snowflake_1_idx,
                                               atlas_snowflake_2_idx};
                renderer.drawSprite(kFlakes[h % 3], x, y, 0.95f, eye, 1.0f);
            } else {
                C2D_DrawRectSolid(x, y, 0.0f, 1.5f, 7.0f, C2D_Color32(150, 180, 235, 150));
            }
        }
    }
}

void WorldScene::drawBuildGhost(const platform::Renderer& renderer, int eye) const {
    core::Vec2f offset = facingOffset();
    int32_t tx = static_cast<int32_t>(std::floor(state_->playerPos.x + offset.x));
    int32_t ty = static_cast<int32_t>(std::floor(state_->playerPos.y + offset.y));
    core::ItemId item = kBuildables[buildGhostIdx_].item;
    bool valid = canPlaceTerrain(item, tx, ty) && canAffordCost(kBuildables[buildGhostIdx_].cost);

    float camX = state_->playerPos.x, camY = state_->playerPos.y;
    float sx = (static_cast<float>(tx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
    float sy = (static_cast<float>(ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;

    uint32_t rectColor = valid ? C2D_Color32(0x50, 0xE0, 0x50, 0x60) : C2D_Color32(0xE0, 0x50, 0x50, 0x60);
    C2D_DrawRectSolid(sx + platform::stereoShift(0.02f, eye), sy, 0.0f, kScreenTilePx, kScreenTilePx, rectColor);

    float offX, offY;
    footprintOffsetForItem(item, &offX, &offY);
    uint32_t tint = valid ? C2D_Color32(0x60, 0xFF, 0x60, 0xFF) : C2D_Color32(0xFF, 0x60, 0x60, 0xFF);
    renderer.drawSpriteTinted(spriteForItem(item), sx + offX, sy + offY, 0.65f, eye, kSpriteScale, tint, 0.4f);
}

void WorldScene::contextPrompts(char* aOut, size_t aN, char* bOut, size_t bN) const {
    aOut[0] = 0;
    bOut[0] = 0;
    if (riding_) {
        snprintf(aOut, aN, "Hop out");
        return;
    }
    if (swimming_) return;
    if (fishState_ == 2) {
        snprintf(aOut, aN, "Reel it in!");
        return;
    }
    if (fishState_ == 1) {
        snprintf(aOut, aN, "Wait for it...");
        return;
    }

    core::Vec2f off = facingOffset();
    int32_t tx = static_cast<int32_t>(std::floor(state_->playerPos.x + off.x));
    int32_t ty = static_cast<int32_t>(std::floor(state_->playerPos.y + off.y));
    const core::Tile& tile = state_->world.tileAt(tx, ty);
    int64_t now = core::nowSeconds();
    const core::ItemStack& sel = selectedStack();

    // B mirrors doDigAction.
    if (tile.placed != core::Placed::None) {
        bool loadBearing = false;
        if (inInterior() &&
            (tile.placed == core::Placed::Wall || tile.placed == core::Placed::Door)) {
            if (core::InteriorData* room = roomContaining(tx, ty)) {
                int32_t ax, ay;
                interiorAnchor(room->bx, room->by, &ax, &ay);
                loadBearing = tx == ax - room->wl || tx == ax + room->wr || ty == ay ||
                              ty == ay + room->h - 1;
            }
        }
        if (!loadBearing) snprintf(bOut, bN, "Demolish");
    } else if (tile.tilled && !tile.hasCrop) {
        snprintf(bOut, bN, "Un-till");
    } else if (tile.terrain == core::Terrain::Hole) {
        snprintf(bOut, bN, "Fill");
    } else if (core::isPathTerrain(tile.terrain) || tile.terrain == core::Terrain::Floor) {
        snprintf(bOut, bN, "Dig up");
    } else if (tile.decoration == core::Decoration::None && !tile.hasCrop &&
               (tile.terrain == core::Terrain::Grass || tile.terrain == core::Terrain::Dirt)) {
        snprintf(bOut, bN, "Dig");
    }

    // A mirrors doContextualAction's priority order.
    if (state_->groundItemAt(tx, ty)) {
        snprintf(aOut, aN, "Pick up");
        return;
    }
    if (state_->clone.exists) {
        float cdx = state_->clone.pos.x - (static_cast<float>(tx) + 0.5f);
        float cdy = state_->clone.pos.y - (static_cast<float>(ty) + 0.5f);
        if (cdx * cdx + cdy * cdy < 1.4f) {
            snprintf(aOut, aN, "Instruct clone");
            return;
        }
    }
    switch (tile.placed) {
        case core::Placed::MineShaft: snprintf(aOut, aN, "Descend"); return;
        case core::Placed::Chest: snprintf(aOut, aN, "Open"); return;
        case core::Placed::Coop:
        case core::Placed::Barn:
        case core::Placed::Camp:
        case core::Placed::Cottage:
        case core::Placed::Hut:
        case core::Placed::Manor: snprintf(aOut, aN, "Enter"); return;
        case core::Placed::Beehive: snprintf(aOut, aN, "Collect"); return;
        case core::Placed::Wall:
            if (inInterior()) {
                if (core::InteriorData* room = roomContaining(tx, ty)) {
                    core::Placed rk = static_cast<core::Placed>(room->kind);
                    int32_t ax, ay;
                    interiorAnchor(room->bx, room->by, &ax, &ay);
                    if ((rk == core::Placed::Cottage || rk == core::Placed::Hut ||
                         rk == core::Placed::Manor) &&
                        (tx == ax - room->wl || tx == ax + room->wr)) {
                        snprintf(aOut, aN, "Expand (%dW %dS)", core::kCostExpand.wood,
                                 core::kCostExpand.stone);
                    }
                }
            } else {
                snprintf(aOut, aN, "Restyle");
            }
            return;
        case core::Placed::Bed: {
            float h = core::dayHour(now);
            snprintf(aOut, aN, (h >= 20.0f || h < 6.0f) ? "Sleep" : "Restyle");
            return;
        }
        case core::Placed::Workbench: snprintf(aOut, aN, "Craft"); return;
        case core::Placed::Chair: snprintf(aOut, aN, "Rotate"); return;
        case core::Placed::Table:
        case core::Placed::Dresser:
        case core::Placed::Stool:
        case core::Placed::Bench: snprintf(aOut, aN, "Swap wood"); return;
        case core::Placed::Rug:
        case core::Placed::RugLong:
        case core::Placed::Lamp:
        case core::Placed::Present: snprintf(aOut, aN, "Restyle"); return;
        case core::Placed::Trough: snprintf(aOut, aN, tile.decoTier ? "Empty hay" : "Add hay"); return;
        case core::Placed::HayBale: snprintf(aOut, aN, "Stack"); return;
        case core::Placed::Mailbox: snprintf(aOut, aN, "Check mail"); return;
        case core::Placed::WaterTray:
            if (sel.item == core::kItemWateringCanFull && tile.decoTier != 0) {
                snprintf(aOut, aN, "Fill tray");
            }
            return;
        default: break;
    }
    for (const WildAnimal& w : wild_) {
        if (w.kind == 2 || w.kind == 3) continue; // frogs & fish: free spirits
        float dx = w.x - (static_cast<float>(tx) + 0.5f);
        float dy = w.y - (static_cast<float>(ty) + 0.5f);
        if (dx * dx + dy * dy < 1.1f) {
            snprintf(aOut, aN, "Feed");
            return;
        }
    }
    if (sel.item == core::kItemHammer) {
        if (tile.decoration == core::Decoration::Tree && core::nodeReady(tile, now)) {
            snprintf(aOut, aN, "Shake");
        } else if (tile.decoration == core::Decoration::Rock) {
            snprintf(aOut, aN, "Bonk");
        } else {
            snprintf(aOut, aN, "Build");
        }
        return;
    }
    if (tile.hasCrop && core::canHarvest(tile, now)) {
        snprintf(aOut, aN, "Harvest");
        return;
    }
    if (tile.decoration == core::Decoration::Tree) {
        if (sel.item == core::kItemAxe) {
            snprintf(aOut, aN, core::nodeReady(tile, now) ? "Chop" : "");
        } else if (clampTier(tile.decoTier) == 2 && core::nodeReady(tile, now) &&
                   core::elapsedAtLeast(tile.timestamp, now, core::kFruitPickCooldownSec)) {
            snprintf(aOut, aN, "Pick fruit");
        }
        return;
    }
    if (tile.decoration == core::Decoration::Rock) {
        if (sel.item == core::kItemPickaxe && core::nodeReady(tile, now)) {
            snprintf(aOut, aN, "Mine");
        }
        return;
    }
    if ((tile.decoration == core::Decoration::Bush ||
         tile.decoration == core::Decoration::Mushroom) &&
        core::nodeReady(tile, now)) {
        snprintf(aOut, aN, "Forage");
        return;
    }
    if (tile.decoration == core::Decoration::Pebble) {
        snprintf(aOut, aN, "Pick up");
        return;
    }
    if ((tile.decoration == core::Decoration::WildPumpkin ||
         tile.decoration == core::Decoration::WildSunflower) &&
        core::nodeReady(tile, now) && wildStageAt(tile, tx, ty, now) == 3) {
        snprintf(aOut, aN, "Forage");
        return;
    }
    if (tile.decoration == core::Decoration::None && tile.placed == core::Placed::None &&
        !tile.hasCrop && bloomSpriteAt(tile, tx, ty, now) >= 0) {
        snprintf(aOut, aN, "Pick flowers");
        return;
    }
    if (sel.item == core::kItemHoe && core::canTill(tile)) {
        snprintf(aOut, aN, "Till");
        return;
    }
    if (sel.item == core::kItemWateringCan &&
        (tile.terrain == core::Terrain::Water || tile.placed == core::Placed::Well)) {
        snprintf(aOut, aN, "Fill can");
        return;
    }
    if (sel.item == core::kItemWateringCanFull) {
        if (tile.hasCrop && !tile.watered) {
            snprintf(aOut, aN, "Water");
        } else if (tile.terrain == core::Terrain::Hole) {
            snprintf(aOut, aN, "Pour");
        }
        return;
    }
    if (sel.item == core::kItemFishingRod && tile.terrain == core::Terrain::Water) {
        snprintf(aOut, aN, "Cast");
        return;
    }
    if (sel.item == core::kItemSapling && tile.terrain == core::Terrain::Hole) {
        snprintf(aOut, aN, "Plant");
        return;
    }
    if (sel.item == core::kItemWood) {
        if (canPlaceTerrain(core::kItemWorkbench, tx, ty)) snprintf(aOut, aN, "Build bench");
        return;
    }
    for (int i = 0; i < core::kCropSpeciesCount; i++) {
        if (sel.item == core::kCropSpeciesTable[i].seedItem) {
            if (core::canPlant(tile)) snprintf(aOut, aN, "Plant");
            return;
        }
    }
    if (sel.item != core::kItemNone &&
        core::kItemTable[sel.item].category == core::ItemCategory::Placeable) {
        if (canPlaceGhost(sel.item, tx, ty)) snprintf(aOut, aN, "Place");
        return;
    }
    int32_t px = static_cast<int32_t>(std::floor(state_->playerPos.x));
    int32_t py = static_cast<int32_t>(std::floor(state_->playerPos.y));
    if (state_->world.tileAt(px, py).terrain == core::Terrain::Rail) {
        snprintf(aOut, aN, "Ride");
    }
}

void WorldScene::drawPromptBar(const platform::Renderer& renderer, int eye, const char* aLbl,
                               const char* bLbl) const {
    // Bottom-left of the top screen: [A] label   [B] label. Drawn at the
    // screen plane (zero stereo depth) - parallax made them hard to read.
    // 16px source at 3x scale = 48px so the glyphs are actually legible.
    float x = 6.0f;
    const float y = 188.0f;
    uint32_t col = C2D_Color32(0xFF, 0xFF, 0xEE, 0xFF);
    if (aLbl[0]) {
        renderer.drawSprite(atlas_ui_btn_a_idx, x, y, 0.0f, eye, 3.0f);
        renderer.drawText(aLbl, x + 54.0f, y + 18.0f, 0.0f, eye, 0.45f, col);
        x += 54.0f + static_cast<float>(std::strlen(aLbl)) * 9.5f + 32.0f;
    }
    if (bLbl[0]) {
        renderer.drawSprite(atlas_ui_btn_b_idx, x, y, 0.0f, eye, 3.0f);
        renderer.drawText(bLbl, x + 54.0f, y + 18.0f, 0.0f, eye, 0.45f, col);
    }
}

void WorldScene::drawSeedGhost(const platform::Renderer& renderer, int eye) const {
    // Same language as the build ghost: with a seed in hand, a tinted
    // stage-0 sprout previews on the faced tile - green where it can be
    // planted (tilled, empty soil), red where it can't. Saplings preview
    // over holes the same way.
    const core::ItemStack& held = selectedStack();
    core::Vec2f offset = facingOffset();
    int32_t tx = static_cast<int32_t>(std::floor(state_->playerPos.x + offset.x));
    int32_t ty = static_cast<int32_t>(std::floor(state_->playerPos.y + offset.y));
    const core::Tile& tile = state_->world.tileAt(tx, ty);

    // Wood in hand previews the Workbench (the hammerless bootstrap
    // build) with the same green/red legality tint.
    if (held.item == core::kItemWood) {
        float camX0 = state_->playerPos.x, camY0 = state_->playerPos.y;
        float gx = (static_cast<float>(tx) - camX0) * kScreenTilePx + kTopScreenW / 2.0f;
        float gy = (static_cast<float>(ty) - camY0) * kScreenTilePx + kTopScreenH / 2.0f;
        bool ok = canPlaceTerrain(core::kItemWorkbench, tx, ty) &&
                  canAffordCost(core::kCostWorkbench);
        uint32_t rc = ok ? C2D_Color32(0x50, 0xE0, 0x50, 0x50) : C2D_Color32(0xE0, 0x50, 0x50, 0x50);
        C2D_DrawRectSolid(gx + platform::stereoShift(0.02f, eye), gy, 0.0f, kScreenTilePx,
                          kScreenTilePx, rc);
        uint32_t tc = ok ? C2D_Color32(0x60, 0xFF, 0x60, 0xFF) : C2D_Color32(0xFF, 0x60, 0x60, 0xFF);
        renderer.drawSpriteTinted(atlas_place_workbench_idx, gx, gy - 32.0f, 0.65f, eye,
                                  kSpriteScale, tc, 0.4f);
        return;
    }

    int spr = -1;
    bool valid = false;
    if (held.item == core::kItemSapling) {
        spr = atlas_prop_sapling_idx;
        valid = tile.terrain == core::Terrain::Hole && tile.placed == core::Placed::None &&
                tile.decoration == core::Decoration::None;
    } else {
        for (int i = 0; i < core::kCropSpeciesCount; i++) {
            if (held.item == core::kCropSpeciesTable[i].seedItem) {
                spr = spriteForCropStage(static_cast<uint8_t>(i), 0);
                valid = core::canPlant(tile);
                break;
            }
        }
    }
    if (spr < 0) return;

    float camX = state_->playerPos.x, camY = state_->playerPos.y;
    float sx = (static_cast<float>(tx) - camX) * kScreenTilePx + kTopScreenW / 2.0f;
    float sy = (static_cast<float>(ty) - camY) * kScreenTilePx + kTopScreenH / 2.0f;

    uint32_t rectColor = valid ? C2D_Color32(0x50, 0xE0, 0x50, 0x50) : C2D_Color32(0xE0, 0x50, 0x50, 0x50);
    C2D_DrawRectSolid(sx + platform::stereoShift(0.02f, eye), sy, 0.0f, kScreenTilePx, kScreenTilePx, rectColor);
    uint32_t tint = valid ? C2D_Color32(0x60, 0xFF, 0x60, 0xFF) : C2D_Color32(0xFF, 0x60, 0x60, 0xFF);
    renderer.drawSpriteTinted(spr, sx, sy, 0.65f, eye, kSpriteScale, tint, 0.4f);
}

void WorldScene::drawTabHeader(const platform::Renderer& renderer) const {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, kBottomW, kTabHeaderH, C2D_Color32(0x14, 0x1a, 0x0e, 0xFF));
    const char* name = hudTab_ == HudTab::Inventory ? "INVENTORY" : "SKILLS";
    char label[32];
    // Real button glyphs (Vryell pack) flanking the tab name: L < name > R.
    // 16px source at 2x scale = 32px, exactly filling the header's height.
    renderer.drawSpriteFlat(atlas_ui_btn_l_idx, 0.0f, 0.0f, 2.0f);
    renderer.drawTextFlat(name, 36.0f, 10.0f, 0.42f, C2D_Color32(0xE8, 0xE8, 0xC0, 0xFF));
    renderer.drawSpriteFlat(atlas_ui_btn_r_idx, 100.0f, 0.0f, 2.0f);

    // What's currently equipped/selected - its own full-width line right
    // under the bar, sized to actually read. With the Hammer equipped it
    // shows what X/Y is cycled to and its cost instead - that's the
    // entire build UI now, no separate tab. Skills tab only: Inventory
    // folds this same info into its tool bar row instead (drawInventoryTab).
    if (hudTab_ == HudTab::Skills) {
        char heldBuf[48];
        if (buildModeActive()) {
            const Buildable& b = kBuildables[buildGhostIdx_];
            if (b.cost.stone > 0) {
                snprintf(heldBuf, sizeof(heldBuf), "Build: %s (%d Wood %d Stone)",
                         core::kItemTable[b.item].name, b.cost.wood, b.cost.stone);
            } else {
                snprintf(heldBuf, sizeof(heldBuf), "Build: %s (%d Wood)", core::kItemTable[b.item].name,
                         b.cost.wood);
            }
        } else {
            const core::ItemStack& held = selectedStack();
            snprintf(heldBuf, sizeof(heldBuf), "%s",
                     held.item != core::kItemNone ? core::kItemTable[held.item].name : "-empty hands-");
        }
        renderer.drawTextFlat(heldBuf, 4.0f, kHeldLineY, 0.45f, C2D_Color32(0xFF, 0xF0, 0xB0, 0xFF));
    }

    // In-game clock + a real weather icon (sun/moon/rain/snow), sized to
    // actually read at a glance and pinned to the top-right corner.
    int64_t now = core::nowSeconds();
    float h = core::dayHour(now);
    int hh = static_cast<int>(h);
    int mm = static_cast<int>((h - static_cast<float>(hh)) * 60.0f);
    bool rain = core::weatherAt(state_->worldSeed, now) == core::Weather::Rain;
    bool isNight = h < 6.0f || h >= 20.0f;
    bool inSnow = core::biomeAt(state_->worldSeed,
                                static_cast<int32_t>(std::floor(state_->playerPos.x)),
                                static_cast<int32_t>(std::floor(state_->playerPos.y))) ==
                  core::Biome::Snow;
    int weatherSprite = rain ? (inSnow ? atlas_weather_snow_idx : atlas_weather_rain_idx)
                             : (isNight ? atlas_weather_moon_idx : atlas_weather_sun_idx);
    snprintf(label, sizeof(label), "%02d:%02d", hh, mm);
    renderer.drawTextFlat(label, 206.0f, 9.0f, 0.42f, C2D_Color32(0xC8, 0xC8, 0xA8, 0xFF));
    // 32px source; scale .85 -> ~27px, pinned to the top-right corner.
    renderer.drawSpriteFlat(weatherSprite, kBottomW - 30.0f, 2.0f, 0.85f);
}

void WorldScene::drawHud(const platform::Renderer& renderer) const {
    renderer.beginBottom(C2D_Color32(0x20, 0x28, 0x18, 0xFF));
    drawTabHeader(renderer);
    switch (hudTab_) {
        case HudTab::Inventory: drawInventoryTab(renderer); break;
        case HudTab::Skills: drawSkillsTab(renderer); break;
    }
}

void WorldScene::drawInventoryTab(const platform::Renderer& renderer) const {
    // Tan dialog-box frame ring around the whole tab body (160x104
    // pre-composed hollow 9-slice, at 2x -> 320x208 spanning y32..240).
    renderer.drawSpriteFlat(atlas_ui_frame_inv_idx, 0.0f, kFrameInvY, 2.0f);

    // Tool bar: one fixed slot per tool kind, same UI-kit slot frames as
    // the general grid below, just smaller. Empty slots (not crafted yet)
    // still show the frame, same convention as the grid.
    for (int i = 0; i < core::kToolSlots; i++) {
        const core::ItemStack& stack = state_->toolBelt.slot(i);
        float x = kToolBarX + static_cast<float>(i) * (kToolSlotPx + kToolBarGap);
        float y = kToolBarY + 1.0f;

        renderer.drawSpriteFlat(i == selectedTool_ ? atlas_ui_slot_sel_idx : atlas_ui_slot_idx,
                                x - 1.0f, y - 1.0f, kToolSlotPx / 28.0f);
        if (stack.item == core::kItemNone) continue;

        float scale = iconScaleForItem(stack.item) * (kToolSlotPx / kSlotPx);
        float size = 16.0f * scale;
        renderer.drawSpriteFlat(spriteForItem(stack.item), x + (kToolSlotPx - size) / 2.0f - 1.0f,
                                y + (kToolSlotPx - size) / 2.0f - 1.0f, scale);
    }

    // What's currently equipped, beside the tool bar - the Hammer's build
    // ghost + cost when it's the one selected (the whole build UI, no
    // separate tab), otherwise just the held item's name. A tool's own
    // highlighted slot already shows which one is equipped, so this reads
    // mostly for non-tool selections (seeds, sapling, produce to plant).
    char heldBuf[48];
    if (buildModeActive()) {
        const Buildable& b = kBuildables[buildGhostIdx_];
        if (b.cost.stone > 0) {
            snprintf(heldBuf, sizeof(heldBuf), "Build: %s (%dW %dS)", core::kItemTable[b.item].name,
                     b.cost.wood, b.cost.stone);
        } else {
            snprintf(heldBuf, sizeof(heldBuf), "Build: %s (%dW)", core::kItemTable[b.item].name,
                     b.cost.wood);
        }
    } else {
        const core::ItemStack& held = selectedStack();
        snprintf(heldBuf, sizeof(heldBuf), "%s",
                 held.item != core::kItemNone ? core::kItemTable[held.item].name : "-empty hands-");
    }
    renderer.drawTextFlat(heldBuf, kToolBarLabelX, kToolBarY + 8.0f, 0.36f,
                          C2D_Color32(0xFF, 0xF0, 0xB0, 0xFF));

    for (int i = 0; i < core::Inventory::slotCount(); i++) {
        const core::ItemStack& stack = state_->inventory.slot(i);
        int col = i % kInvCols;
        int row = i / kInvCols;
        float x = kInvGridX + col * kSlotPx;
        float y = kInvGridY + row * kSlotPx;

        // Real Sprout Lands UI slot frames: raised white = selected.
        renderer.drawSpriteFlat(i == selectedSlot_ ? atlas_ui_slot_sel_idx : atlas_ui_slot_idx,
                                x - 1.0f, y - 1.0f, kSlotPx / 28.0f);
        if (stack.item == core::kItemNone) continue;

        float scale = iconScaleForItem(stack.item);
        float size = 16.0f * scale;
        renderer.drawSpriteFlat(spriteForItem(stack.item), x + (kSlotPx - size) / 2.0f - 1.0f,
                                y + (kSlotPx - size) / 2.0f - 1.0f, scale);

        if (stack.count > 1) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%u", stack.count);
            renderer.drawTextFlat(buf, x + 18.0f, y + 18.0f, 0.45f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
        }
    }

    if (statusFrames_ > 0) {
        renderer.drawTextFlat(statusMsg_, 12.0f, kStatusY, 0.5f, C2D_Color32(0xFF, 0xE8, 0x80, 0xFF));
    }

    // Drop/Trash act on the equipped slot; both dim when hands are empty.
    bool hasHeld = selectedStack().item != core::kItemNone;
    uint32_t btnBg = hasHeld ? C2D_Color32(0x3a, 0x44, 0x28, 0xFF) : C2D_Color32(0x24, 0x28, 0x1c, 0xFF);
    uint32_t btnText = hasHeld ? C2D_Color32(0xE8, 0xE8, 0xD0, 0xFF) : C2D_Color32(0x70, 0x74, 0x64, 0xFF);
    C2D_DrawRectSolid(kDropBtnX, kDropBtnY, 0.0f, kDropBtnW, kDropBtnH, btnBg);
    C2D_DrawRectSolid(kTrashBtnX, kDropBtnY, 0.0f, kTrashBtnW, kDropBtnH, btnBg);
    renderer.drawTextFlat("DROP", kDropBtnX + 14.0f, kDropBtnY + 5.0f, 0.4f, btnText);
    renderer.drawTextFlat("TRASH", kTrashBtnX + 8.0f, kDropBtnY + 5.0f, 0.4f, btnText);

    // Control hints with real button glyphs - 32px (2x scale) so they're
    // actually legible, text vertically centered against them.
    uint32_t hintCol = C2D_Color32(0x90, 0x98, 0x80, 0xFF);
    if (buildModeActive()) {
        renderer.drawSpriteFlat(atlas_ui_btn_x_idx, 12.0f, kHintY, 2.0f);
        renderer.drawSpriteFlat(atlas_ui_btn_y_idx, 48.0f, kHintY, 2.0f);
        renderer.drawTextFlat("cycle", 86.0f, kHintY + 11.0f, 0.34f, hintCol);
        renderer.drawSpriteFlat(atlas_ui_btn_a_idx, 128.0f, kHintY, 2.0f);
        renderer.drawTextFlat("place", 166.0f, kHintY + 11.0f, 0.34f, hintCol);
        renderer.drawSpriteFlat(atlas_ui_btn_b_idx, 214.0f, kHintY, 2.0f);
        renderer.drawTextFlat("demolish", 252.0f, kHintY + 11.0f, 0.34f, hintCol);
    } else {
        renderer.drawSpriteFlat(atlas_ui_btn_a_idx, 12.0f, kHintY, 2.0f);
        renderer.drawTextFlat("use", 50.0f, kHintY + 11.0f, 0.36f, hintCol);
        renderer.drawSpriteFlat(atlas_ui_btn_b_idx, 88.0f, kHintY, 2.0f);
        renderer.drawTextFlat("dig", 126.0f, kHintY + 11.0f, 0.36f, hintCol);
        renderer.drawTextFlat("Tap slot again: unequip", 158.0f, kHintY + 11.0f, 0.36f, hintCol);
    }
}

void WorldScene::drawSkillsTab(const platform::Renderer& renderer) const {
    // Tan dialog-box frame ring around the skill list (160x97 hollow
    // 9-slice at 2x -> 320x194 spanning y46..240, below the held line).
    renderer.drawSpriteFlat(atlas_ui_frame_skills_idx, 0.0f, kFrameSkillsY, 2.0f);

    for (int i = 0; i < core::kSkillCount; i++) {
        float y = kTabContentY + i * kSkillRowH;
        uint32_t xp = state_->skillXp[i];
        int level = core::levelForXp(xp);
        int into, span;
        core::xpProgress(xp, &into, &span);

        char label[40];
        if (xp == 0) {
            // Undiscovered: the row exists (there's more to find!) but
            // nothing about it is disclosed until the player first does it.
            renderer.drawTextFlat("???", 12.0f, y + 1.0f, 0.4f, C2D_Color32(0x70, 0x78, 0x64, 0xFF));
            continue;
        }
        snprintf(label, sizeof(label), "%s", core::kSkillNames[i]);
        renderer.drawTextFlat(label, 12.0f, y + 1.0f, 0.4f, C2D_Color32(0xD8, 0xE8, 0xC0, 0xFF));
        snprintf(label, sizeof(label), "Lv %d", level);
        renderer.drawTextFlat(label, 12.0f, y + 13.0f, 0.32f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
        // A gold star every 5 levels, a half star at +2 or more toward
        // the next (premium UI star icons) - glanceable mastery.
        int stars = level / 5;
        bool half = (level % 5) >= 2;
        for (int s = 0; s < 3; s++) {
            int spr = s < stars ? atlas_ui_star_idx
                      : (s == stars && half) ? atlas_ui_star_half_idx
                                             : atlas_ui_star_empty_idx;
            renderer.drawSpriteFlat(spr, 48.0f + s * 15.0f, y + 12.0f, 0.9f);
        }

        float barH = 10.0f;
        C2D_DrawRectSolid(kSkillBarX, y + 3.0f, 0.0f, kSkillBarW, barH, C2D_Color32(0x14, 0x18, 0x10, 0xFF));
        float fill = span > 0 ? static_cast<float>(into) / static_cast<float>(span) : 0.0f;
        C2D_DrawRectSolid(kSkillBarX, y + 3.0f, 0.0f, kSkillBarW * fill, barH, C2D_Color32(0x88, 0xC8, 0x40, 0xFF));
        snprintf(label, sizeof(label), "%d / %d XP", into, span);
        renderer.drawTextFlat(label, kSkillBarX + 4.0f, y + 14.0f, 0.3f, C2D_Color32(0xB8, 0xC8, 0xA0, 0xFF));
    }
}

void WorldScene::drawChestUi(const platform::Renderer& renderer) const {
    renderer.beginBottom(C2D_Color32(0x1a, 0x16, 0x0e, 0xFF));

    const core::ChestData* chest = nullptr;
    for (const core::ChestData& c : state_->chests) {
        if (c.x == chestX_ && c.y == chestY_) {
            chest = &c;
            break;
        }
    }
    if (!chest) return;

    renderer.drawTextFlat("Chest (tap to move, B: close)", 16.0f, 2.0f, 0.42f,
                          C2D_Color32(0xE8, 0xD8, 0xA0, 0xFF));

    auto drawGrid = [&](const core::Inventory& inv, float startY, bool highlightSelected) {
        for (int i = 0; i < core::Inventory::slotCount(); i++) {
            int col = i % kInvCols;
            int row = i / kInvCols;
            float x = kChestGridX + col * kChestSlotPx;
            float y = startY + row * kChestSlotPx;
            renderer.drawSpriteFlat((highlightSelected && i == selectedSlot_)
                                        ? atlas_ui_slot_sel_idx
                                        : atlas_ui_slot_idx,
                                    x - 1.0f, y - 1.0f, kChestSlotPx / 28.0f);
            const core::ItemStack& s = inv.slot(i);
            if (s.item == core::kItemNone) continue;
            float scale = iconScaleForItem(s.item) * 0.9f;
            float size = 16.0f * scale;
            renderer.drawSpriteFlat(spriteForItem(s.item), x + (kChestSlotPx - size) / 2.0f - 1.0f,
                                    y + (kChestSlotPx - size) / 2.0f - 1.0f, scale);
            if (s.count > 1) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%u", s.count);
                renderer.drawTextFlat(buf, x + 19.0f, y + 19.0f, 0.42f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
            }
        }
    };
    drawGrid(chest->items, kChestTopY, false);
    drawGrid(state_->inventory, kChestBotY, true);
}

void WorldScene::drawPauseMenu(const platform::Renderer& renderer) const {
    for (int eye = 0; eye < 2; eye++) {
        renderer.beginTop(eye, C2D_Color32(0x10, 0x10, 0x18, 0xFF));
        for (int i = 0; i < kPauseOptionCount; i++) {
            uint32_t color = i == pauseSelection_ ? C2D_Color32(0xFF, 0xE8, 0x80, 0xFF)
                                                  : C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
            renderer.drawText(kPauseOptions[i], 150.0f, 80.0f + i * 24.0f, 0.3f, eye, 0.6f, color);
        }
    }
    renderer.beginBottom(C2D_Color32(0x10, 0x10, 0x18, 0xFF));
}

} // namespace scenes
