#pragma once

#include <cstdint>

namespace core {

enum class Terrain : uint8_t {
    Grass = 0,
    Dirt = 1,
    Water = 2,
    Hole = 3, // dug with the shovel; fill with water (pond) or a sapling (tree)
    Path = 4, // placed stone path - player walks faster on it
    Floor = 5, // placed wooden house floor (modular building)
    // Appended (save-safe): the rest of the path family. All walk faster,
    // all shovel back to dirt, all connection-drawn.
    PathDirt = 6,  // plain soil path
    PathPlank = 7, // soil path with laid planks
    Rail = 8,      // cart track on grass - a special kind of path
};

// Every terrain the player lays down to walk faster on (rails included).
inline bool isPathTerrain(Terrain t) {
    return t == Terrain::Path || t == Terrain::PathDirt || t == Terrain::PathPlank ||
           t == Terrain::Rail;
}

// Resource nodes scattered by worldgen (plus player-planted trees). Each
// has a tier 0..2 - higher tiers are procedurally recolored variants that
// appear farther from spawn, need a higher skill level, and give more.
enum class Decoration : uint8_t {
    None = 0,
    Tree = 1,     // tier 0 = small tree, 1 = big 2-column tree, 2 = autumn big
    Rock = 2,     // grey / copper / gold
    Bush = 3,     // berry bush recolors
    Mushroom = 4, // pink / purple / teal
    // Wild crop patches (appended, save-safe): pumpkins and sunflowers
    // growing in organic clumps out in the wild - forage them toolless
    // for the crop (and sometimes a seed). See chunk.cpp's wildPatchAt.
    WildPumpkin = 5,
    WildSunflower = 6,
    // Hedge-maze garden walls (chunk.cpp's mazeAt). Solid, ungatherable,
    // and never regrow-walkable - a maze wall stays a wall.
    Hedge = 7,
    // A loose pebble: tiny, non-blocking, picked up with A for a stone
    // (then gone). Scattered by worldgen and by the occasional-spawn
    // sweep. Rocks themselves no longer regrow - mined is gone.
    Pebble = 8,
};

// Player-built structures occupying a tile. Bridge, Rug, Gate, and
// GateRight are walkable; everything else blocks movement.
enum class Placed : uint8_t {
    None = 0,
    Camp = 1, // placing one sets/moves the home-teleport anchor
    Fence = 2,
    Coop = 3,
    Barn = 4,
    Chest = 5, // per-chest contents live in GameState::chests
    Bridge = 6, // placed on Water; makes it walkable
    Lamp = 7,
    Chair = 8,
    Rug = 9,
    Gate = 10,    // a fence segment the player can walk through (left leaf)
    Well = 11,    // refill the Watering Can away from open water
    Beehive = 12, // produces Honey on a real-time clock (GameState::hives)
    Campfire = 13,
    Sign = 14,
    Mailbox = 15,
    Wall = 16, // modular house wall (blocks; tiles seamlessly in runs)
    Door = 17, // a wall segment the player can walk through
    XmasTree = 18,
    MineShaft = 19, // press A on it to descend into the Mine
    // Village-pack grand homes + winter fun (all decorative).
    Cottage = 20,
    Hut = 21,
    Manor = 22,
    Snowman = 23,
    // Appended so existing saves' Placed values stay valid: the gate's
    // right leaf, placed as its own separate piece from the left (Gate).
    GateRight = 24,
    Roof = 25, // shingled roof section (blocks; connects east/west in runs)
    // Furniture drop: long rugs, beds, and tables. For placed furniture
    // the tile's decoTier byte (unused when decoration == None) stores
    // the variant - chair facing, rug/bed color, table wood - cycled by
    // pressing A on the placed piece. Already serialized, no save bump.
    RugLong = 26, // 2-tile-wide rug (walkable, like Rug)
    Bed = 27,
    Table = 28,
    Dresser = 29,
    Stool = 30,
    Bench = 31, // 2-tile-wide seat (blocks; art spills onto the east tile)
    Workbench = 32, // crafting station - A opens the tool-crafting menu
    // Homestead update (appended, save-safe). decoTier stores the variant
    // as usual: trough empty/full, hay bale small/long, tray fill level,
    // present color. Chest tiers reuse Placed::Chest with decoTier 4/5.
    Trough = 33,
    HayBale = 34,
    WaterTray = 35, // art spills onto the east tile (32px wide)
    Boat = 36,      // water-only, like Bridge (but blocks - it's moored)
    Picnic = 37,    // blanket spread; art spills east (48px wide)
    Present = 38,
};

struct Tile {
    Terrain terrain = Terrain::Grass;
    bool tilled = false;
    bool hasCrop = false;
    // Watered crops grow 1.5x faster (see cropStageOf); cleared on
    // plant/harvest.
    bool watered = false;
    uint8_t cropSpeciesId = 0;
    // Shared real-time anchor (unix seconds). A tile is never a crop AND a
    // node at once, so one field serves both: with hasCrop it's planted-at;
    // with a depleted node it's when it was gathered (or when a sapling was
    // planted) - the node comes back once the respawn time elapses.
    int64_t timestamp = 0;
    Decoration decoration = Decoration::None;
    uint8_t decoTier = 0;
    bool depleted = false;
    Placed placed = Placed::None;
};

// --- Legality helpers (pure terrain rules, no clock/species knowledge) ----

inline bool canTill(const Tile& tile) {
    return !tile.tilled && tile.decoration == Decoration::None &&
           tile.placed == Placed::None &&
           (tile.terrain == Terrain::Grass || tile.terrain == Terrain::Dirt);
}

inline bool tillTile(Tile& tile) {
    if (!canTill(tile)) return false;
    tile.tilled = true;
    return true;
}

// `waterFrozen` = this tile sits in the Snowlands, where open water is
// walkable ice (a per-region fact the caller knows; tiles don't store it).
// With a nonzero `now`, felled/foraged nodes are walkable while they
// regrow (a stump isn't a wall); with now == 0 every node blocks - the
// conservative default for callers with no clock (worldgen spawn etc.).
bool blocksMovement(const Tile& tile, bool waterFrozen = false, int64_t now = 0);

// Can a standard placeable (fence/camp/coop/barn/chest/lamp/chair/rug) go
// here? Bridges have their own rule (water only).
bool canPlace(const Tile& tile);
bool canPlaceBridge(const Tile& tile);

// --- Shovel / bucket terrain editing ---------------------------------------

enum class ShovelResult : uint8_t {
    Blocked,  // nothing shovel-able here
    Untilled, // tilled soil reverted to plain
    Dug,      // Grass/Dirt -> Hole
    Filled,   // Hole -> Dirt
    Removed,  // placed object demolished (caller checks chest-empty first)
};
ShovelResult useShovel(Tile& tile);

bool pourWater(Tile& tile);  // full bucket onto a Hole -> Water
bool plantSapling(Tile& tile, int64_t now); // sapling into a Hole -> growing tree

// --- Resource-node gathering ------------------------------------------------

// A node exists and is currently harvestable (not chopped/gathered and
// waiting on its respawn timer, and not a still-growing planted sapling).
bool nodeReady(const Tile& tile, int64_t now);

enum class GatherResult : uint8_t {
    NotANode,
    Regrowing,
    LevelTooLow,
    Ok,
};
// Attempts to gather the node on this tile (caller aims the right tool at
// the right Decoration kind - `expected` - and passes the player's level in
// that node's skill). On Ok the node is marked depleted with its respawn
// clock started; yields/XP are the caller's job (see balance.h).
// `levelReq` is supplied by the caller because it can be variant-aware
// (each tree variation has its own requirement - see balance.h's
// treeLevelReq); plain kinds just pass their NodeBalance levelReq[tier].
GatherResult gatherNode(Tile& tile, Decoration expected, int skillLevel, int levelReq,
                        int64_t now);

} // namespace core
