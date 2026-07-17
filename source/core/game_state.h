#pragma once

#include <cstdint>
#include <vector>

#include "core/balance.h"
#include "core/inventory.h"
#include "core/skills.h"
#include "core/world.h"

namespace core {

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

enum class Facing : uint8_t { Down = 0, Up = 1, Left = 2, Right = 3 };

// World coordinates of the current Camp (home). Unset until the first Camp
// is placed - teleport falls back to spawn (0,0) until then.
struct HomeAnchor {
    bool set = false;
    int32_t x = 0;
    int32_t y = 0;
};

enum class AnimalSpecies : uint8_t { Chicken = 0, Cow = 1 };

// A tamed animal lives at the Coop/Barn tile it was assigned to when tamed
// and produces on a real-time clock (egg/milk) - collect by pressing A on
// its building. It starts as a BABY (no produce yet) and grows up after
// kBabyGrowSec of real time. `variant` picks the color art (0 = the
// classic free-pack look, 1-4 = the premium recolors), rolled when the
// wild animal originally spawned.
struct TamedAnimal {
    AnimalSpecies species = AnimalSpecies::Chicken;
    uint8_t variant = 0;
    int32_t homeX = 0;
    int32_t homeY = 0;
    int64_t tamedAt = 0;
    int64_t lastCollectedAt = 0;
};

// Contents of one placed chest, keyed by its tile.
struct ChestData {
    int32_t x = 0;
    int32_t y = 0;
    Inventory items;
};

// One placed Beehive's honey clock, keyed by its tile.
struct HiveData {
    int32_t x = 0;
    int32_t y = 0;
    int64_t lastCollectedAt = 0;
};

// A dropped item stack sitting on the ground, keyed by its tile - the
// result of deliberately dropping something from the inventory. Pick it
// back up with A; it never expires.
struct GroundItem {
    int32_t x = 0;
    int32_t y = 0;
    ItemId item = kItemNone;
    uint16_t count = 0;
};

// One building's interior room, keyed by the building's overworld tile.
// The room itself is ordinary stamped tiles (Floor terrain, Wall ring, a
// Door in the south wall) living in a reserved band far south of the
// overworld - see WorldScene's interior helpers - so furniture, chests,
// and saving all Just Work inside. Sizes include the wall ring; homes
// grow east/west via wl/wr (half-widths either side of the anchor
// column, total width = wl + 1 + wr).
struct InteriorData {
    int32_t bx = 0; // building tile (overworld)
    int32_t by = 0;
    uint8_t kind = 0; // core::Placed of the building
    uint8_t wl = 0;   // columns west of the anchor/door column
    uint8_t wr = 0;   // columns east of it
    uint8_t h = 0;    // rows, walls included
};

// The Clone Mirror's crystal double: at most one exists. It stands where
// it was built, takes orders via A (task cycles Rest -> Lumberjack ->
// Miner -> Forager -> Farmer), and works nearby nodes/beds, depositing
// yields into the nearest chest. Transient work state (target, cooldown)
// lives in WorldScene; only this persists.
struct CloneData {
    bool exists = false;
    Vec2f pos;
    uint8_t task = 0; // 0 rest, 1 lumber, 2 mine, 3 forage, 4 farm
};

// Everything a save file needs to round-trip. Bundled together (rather than
// scattered across globals) so save.cpp has one thing to serialize and
// world_scene has one thing to carry around.
struct GameState {
    explicit GameState(uint32_t seed) : worldSeed(seed), world(seed) {}

    uint32_t worldSeed;
    Vec2f playerPos;
    Facing facing = Facing::Down;
    HomeAnchor home;

    // The there-and-back half of the home/teleport toggle - see the plan's
    // "Home / teleport" section. Unset until the player's first teleport.
    bool hasLastFieldPos = false;
    Vec2f lastFieldPos;

    // Lifetime XP per skill (level is derived - see skills.h).
    uint32_t skillXp[kSkillCount] = {};

    // Accumulated bed-sleep fast-forward, in real seconds - applied to the
    // wall clock via core::setClockOffset() on load and grown each time
    // the player sleeps to morning. Only ever increases.
    int64_t clockOffset = 0;

    // Sprint fuel (v12), spent by running/hard-swimming and refilled by
    // easing off - see the stamina block in balance.h. The max is derived:
    // kStaminaBase + kStaminaPerAthleticsLevel per Athletics level above 1.
    float stamina = kStaminaBase;

    Inventory inventory;
    ToolBelt toolBelt;
    std::vector<TamedAnimal> animals;
    std::vector<ChestData> chests;
    std::vector<HiveData> hives;
    std::vector<GroundItem> groundItems;
    std::vector<InteriorData> interiors;
    CloneData clone;
    ChunkStore world;

    int skillLevel(Skill s) const { return levelForXp(skillXp[static_cast<int>(s)]); }

    // Finds the chest/hive/ground-item/interior at a tile, or nullptr.
    ChestData* chestAt(int32_t x, int32_t y);
    HiveData* hiveAt(int32_t x, int32_t y);
    GroundItem* groundItemAt(int32_t x, int32_t y);
    InteriorData* interiorAt(int32_t bx, int32_t by);

    // New-game bootstrap: the four tools and nothing else. Seeds come from
    // foraging bushes, saplings from chopping trees, animals from taming -
    // nothing else is handed to the player.
    static GameState newGame(uint32_t seed);
};

} // namespace core
