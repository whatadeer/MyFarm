// Host-native tests for source/core/tile.h/.cpp: till legality, movement
// blocking, shovel/bucket terrain editing, sapling planting, and
// resource-node gathering. Planting/harvesting crops (which need the crop
// species table) are tested in test_crop.cpp instead.
#include "core/balance.h"
#include "core/tile.h"
#include "minitest.h"

using namespace core;

static void test_till_grass_succeeds() {
    Tile tile;
    tile.terrain = Terrain::Grass;
    CHECK(canTill(tile));
    CHECK(tillTile(tile));
    CHECK(tile.tilled);
}

static void test_till_water_hole_path_rejected() {
    Tile tile;
    tile.terrain = Terrain::Water;
    CHECK(!canTill(tile));
    tile.terrain = Terrain::Hole;
    CHECK(!canTill(tile));
    tile.terrain = Terrain::Path;
    CHECK(!canTill(tile));
}

static void test_till_blocked_by_decoration_and_placed() {
    Tile tile;
    tile.decoration = Decoration::Bush;
    CHECK(!canTill(tile));
    tile.decoration = Decoration::None;
    tile.placed = Placed::Fence;
    CHECK(!canTill(tile));
    tile.placed = Placed::None;
    CHECK(canTill(tile));
}

static void test_blocks_movement() {
    Tile tile;
    CHECK(!blocksMovement(tile)); // plain grass
    tile.terrain = Terrain::Water;
    CHECK(blocksMovement(tile));
    tile.placed = Placed::Bridge;
    CHECK(!blocksMovement(tile)); // bridged water is walkable
    tile.placed = Placed::None;
    tile.terrain = Terrain::Hole;
    CHECK(blocksMovement(tile));
    tile.terrain = Terrain::Grass;
    tile.decoration = Decoration::Tree;
    CHECK(blocksMovement(tile));
    tile.decoration = Decoration::None;
    tile.placed = Placed::Fence;
    CHECK(blocksMovement(tile));
    tile.placed = Placed::Rug;
    CHECK(!blocksMovement(tile)); // rugs are floor decor
    tile.placed = Placed::Gate;
    CHECK(!blocksMovement(tile)); // gates are the walk-through fence piece
    tile.placed = Placed::Well;
    CHECK(blocksMovement(tile));
    tile.placed = Placed::Wall;
    CHECK(blocksMovement(tile));
    tile.placed = Placed::Door;
    CHECK(!blocksMovement(tile)); // doors are the walk-through wall piece
    // Snowlands: open water freezes into walkable ice.
    Tile pond;
    pond.terrain = Terrain::Water;
    CHECK(blocksMovement(pond, false));
    CHECK(!blocksMovement(pond, true));
}

static void test_floor_building_rules() {
    Tile tile;
    tile.terrain = Terrain::Floor;
    CHECK(!blocksMovement(tile));  // walkable interior
    CHECK(canPlace(tile));         // walls/furniture go on floor
    CHECK(!canTill(tile));         // but it's not farmland
    CHECK(useShovel(tile) == ShovelResult::Untilled); // shovel rips it up
    CHECK(tile.terrain == Terrain::Dirt);
}

static void test_shovel_dig_fill_cycle() {
    Tile tile;
    CHECK(useShovel(tile) == ShovelResult::Dug);
    CHECK(tile.terrain == Terrain::Hole);
    CHECK(useShovel(tile) == ShovelResult::Filled);
    CHECK(tile.terrain == Terrain::Dirt);
    CHECK(useShovel(tile) == ShovelResult::Dug); // dirt digs again
}

static void test_shovel_untills_before_digging() {
    Tile tile;
    tillTile(tile);
    CHECK(useShovel(tile) == ShovelResult::Untilled);
    CHECK(!tile.tilled);
    CHECK(tile.terrain == Terrain::Dirt);
}

static void test_shovel_blocked_by_crop_and_nodes() {
    Tile tile;
    tillTile(tile);
    tile.hasCrop = true;
    CHECK(useShovel(tile) == ShovelResult::Blocked);

    Tile node;
    node.decoration = Decoration::Rock;
    CHECK(useShovel(node) == ShovelResult::Blocked);
}

static void test_shovel_removes_placed() {
    Tile tile;
    tile.placed = Placed::Fence;
    CHECK(useShovel(tile) == ShovelResult::Removed);
    CHECK(tile.placed == Placed::None);
}

static void test_water_from_bucket_makes_pond() {
    Tile tile;
    CHECK(!pourWater(tile)); // not a hole
    useShovel(tile);
    CHECK(pourWater(tile));
    CHECK(tile.terrain == Terrain::Water);
    CHECK(blocksMovement(tile));
}

static void test_sapling_only_in_hole_then_grows() {
    Tile tile;
    CHECK(!plantSapling(tile, 1000)); // not a hole
    useShovel(tile);
    CHECK(plantSapling(tile, 1000));
    CHECK(tile.terrain == Terrain::Dirt);
    CHECK(tile.decoration == Decoration::Tree);
    CHECK(tile.depleted); // still growing

    CHECK(!nodeReady(tile, 1000 + kSaplingGrowSec - 1));
    CHECK(nodeReady(tile, 1000 + kSaplingGrowSec)); // matured into a choppable tree
}

static void test_gather_lifecycle_and_respawn() {
    Tile tile;
    tile.decoration = Decoration::Tree;
    tile.decoTier = 0;

    CHECK(gatherNode(tile, Decoration::Rock, 1, 1, 1000) == GatherResult::NotANode); // wrong kind
    CHECK(gatherNode(tile, Decoration::Tree, 1, 1, 1000) == GatherResult::Ok);
    CHECK(tile.depleted);
    CHECK(gatherNode(tile, Decoration::Tree, 1, 1, 1000 + 5) == GatherResult::Regrowing);

    int64_t respawned = 1000 + kTreeBalance.respawnSec[0];
    CHECK(gatherNode(tile, Decoration::Tree, 1, 1, respawned) == GatherResult::Ok);
}

static void test_gather_gated_by_skill_level() {
    Tile tile;
    tile.decoration = Decoration::Rock;
    tile.decoTier = 2; // a boulder - the caller passes its variant req
    CHECK(gatherNode(tile, Decoration::Rock, kRockBalance.levelReq[2] - 1,
                     kRockBalance.levelReq[2], 1000) == GatherResult::LevelTooLow);
    CHECK(!tile.depleted); // failed attempt consumes nothing
    CHECK(gatherNode(tile, Decoration::Rock, kRockBalance.levelReq[2], kRockBalance.levelReq[2],
                     1000) == GatherResult::Ok);
}

static void test_place_legality() {
    Tile tile;
    CHECK(canPlace(tile));
    tile.tilled = true;
    CHECK(!canPlace(tile));
    tile.tilled = false;
    tile.decoration = Decoration::Mushroom;
    CHECK(!canPlace(tile));
    tile.decoration = Decoration::None;
    tile.terrain = Terrain::Water;
    CHECK(!canPlace(tile));
    CHECK(canPlaceBridge(tile));
    tile.placed = Placed::Bridge;
    CHECK(!canPlaceBridge(tile)); // one bridge per tile
}

int main() {
    printf("test_tile:\n");
    RUN(test_till_grass_succeeds);
    RUN(test_till_water_hole_path_rejected);
    RUN(test_till_blocked_by_decoration_and_placed);
    RUN(test_blocks_movement);
    RUN(test_floor_building_rules);
    RUN(test_shovel_dig_fill_cycle);
    RUN(test_shovel_untills_before_digging);
    RUN(test_shovel_blocked_by_crop_and_nodes);
    RUN(test_shovel_removes_placed);
    RUN(test_water_from_bucket_makes_pond);
    RUN(test_sapling_only_in_hole_then_grows);
    RUN(test_gather_lifecycle_and_respawn);
    RUN(test_gather_gated_by_skill_level);
    RUN(test_place_legality);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
