// Host-native tests for source/core/save.h: full round-trip through the
// byte buffer (no file I/O - that's platform/save_io.cpp, a thin sdmc
// wrapper not worth testing without a real 3DS), and rejection of anything
// that doesn't look like a well-formed save.
#include "core/crop.h"
#include "core/save.h"
#include "minitest.h"

using namespace core;

static void test_round_trip_preserves_everything() {
    GameState state = GameState::newGame(12345);
    state.playerPos = {12.5f, -3.25f};
    state.facing = Facing::Left;
    state.home.set = true;
    state.home.x = 100;
    state.home.y = -50;
    state.hasLastFieldPos = true;
    state.lastFieldPos = {7.0f, 8.0f};
    state.skillXp[static_cast<int>(Skill::Logging)] = 777;
    state.skillXp[static_cast<int>(Skill::Herding)] = 42;
    state.clockOffset = 123456; // bed-sleep fast-forward (v7)

    TamedAnimal hen;
    hen.species = AnimalSpecies::Chicken;
    hen.variant = 3;
    hen.homeX = 9;
    hen.homeY = -2;
    hen.tamedAt = 4200;
    hen.lastCollectedAt = 5000;
    state.animals.push_back(hen);

    ChestData chest;
    chest.x = 4;
    chest.y = 4;
    chest.items.add(kItemWood, 30);
    state.chests.push_back(chest);

    HiveData hive;
    hive.x = -7;
    hive.y = 12;
    hive.lastCollectedAt = 8888;
    state.hives.push_back(hive);

    GroundItem dropped;
    dropped.x = 15;
    dropped.y = -3;
    dropped.item = kItemDiamond;
    dropped.count = 2;
    state.groundItems.push_back(dropped);

    // Touch tiles in two different chunks so both become dirty and get saved.
    Tile& t1 = state.world.tileAt(3, 3);
    t1.decoration = Decoration::None; // worldgen may have put a node here
    tillTile(t1);
    plantCrop(t1, kCropWheat, 1000);
    state.world.markDirty(3, 3);

    Tile& t2 = state.world.tileAt(-20, 40);
    t2.decoration = Decoration::Bush;
    t2.decoTier = 2;
    t2.depleted = true;
    t2.timestamp = 2222;
    t2.placed = Placed::None;
    state.world.markDirty(-20, 40);

    std::vector<uint8_t> bytes = serializeSave(state);

    GameState loaded(0); // seed gets overwritten by deserializeSave
    CHECK(deserializeSave(bytes, &loaded));

    CHECK(loaded.worldSeed == 12345);
    CHECK(loaded.playerPos.x == 12.5f && loaded.playerPos.y == -3.25f);
    CHECK(loaded.facing == Facing::Left);
    CHECK(loaded.home.set && loaded.home.x == 100 && loaded.home.y == -50);
    CHECK(loaded.hasLastFieldPos);
    CHECK(loaded.lastFieldPos.x == 7.0f && loaded.lastFieldPos.y == 8.0f);
    CHECK(loaded.skillXp[static_cast<int>(Skill::Logging)] == 777);
    CHECK(loaded.skillXp[static_cast<int>(Skill::Herding)] == 42);
    CHECK(loaded.clockOffset == 123456);

    // Axe-only starter kit from newGame() should have round-tripped too -
    // into the v11 tool belt, not the general grid (every other tool is
    // crafted at the Workbench now).
    CHECK(loaded.toolBelt.countOf(kItemAxe) == 1);
    CHECK(loaded.toolBelt.countOf(kItemHoe) == 0);
    CHECK(loaded.toolBelt.countOf(kItemHammer) == 0);
    CHECK(loaded.inventory.countOf(kItemWheatSeed) == 0); // no free seeds either

    CHECK(loaded.animals.size() == 1);
    CHECK(loaded.animals[0].species == AnimalSpecies::Chicken);
    CHECK(loaded.animals[0].variant == 3);
    CHECK(loaded.animals[0].homeX == 9 && loaded.animals[0].homeY == -2);
    CHECK(loaded.animals[0].tamedAt == 4200);
    CHECK(loaded.animals[0].lastCollectedAt == 5000);

    CHECK(loaded.chests.size() == 1);
    CHECK(loaded.chests[0].x == 4 && loaded.chests[0].y == 4);
    CHECK(loaded.chests[0].items.countOf(kItemWood) == 30);

    CHECK(loaded.hives.size() == 1);
    CHECK(loaded.hives[0].x == -7 && loaded.hives[0].y == 12);
    CHECK(loaded.hives[0].lastCollectedAt == 8888);

    CHECK(loaded.groundItems.size() == 1);
    CHECK(loaded.groundItems[0].x == 15 && loaded.groundItems[0].y == -3);
    CHECK(loaded.groundItems[0].item == kItemDiamond);
    CHECK(loaded.groundItems[0].count == 2);

    Tile& loadedT1 = loaded.world.tileAt(3, 3);
    CHECK(loadedT1.tilled);
    CHECK(loadedT1.hasCrop);
    CHECK(loadedT1.cropSpeciesId == kCropWheat);
    CHECK(loadedT1.timestamp == 1000);

    Tile& loadedT2 = loaded.world.tileAt(-20, 40);
    CHECK(loadedT2.decoration == Decoration::Bush);
    CHECK(loadedT2.decoTier == 2);
    CHECK(loadedT2.depleted);
    CHECK(loadedT2.timestamp == 2222);
}

static void test_unmodified_chunk_regenerates_identically() {
    GameState state = GameState::newGame(1);
    Tile original = state.world.tileAt(0, 0); // read-only touch, never marked dirty
    std::vector<uint8_t> bytes = serializeSave(state);

    GameState loaded(0);
    CHECK(deserializeSave(bytes, &loaded));

    Tile reloaded = loaded.world.tileAt(0, 0);
    CHECK(original.terrain == reloaded.terrain);
    CHECK(original.decoration == reloaded.decoration);
}

static void test_bad_magic_rejected() {
    std::vector<uint8_t> bytes = serializeSave(GameState::newGame(1));
    bytes[0] = 'X';
    GameState loaded(0);
    CHECK(!deserializeSave(bytes, &loaded));
}

static void test_version_mismatch_rejected() {
    std::vector<uint8_t> bytes = serializeSave(GameState::newGame(1));
    bytes[4] = 99; // version byte immediately follows the 4-byte magic
    GameState loaded(0);
    CHECK(!deserializeSave(bytes, &loaded));
}

static void test_truncated_buffer_rejected() {
    std::vector<uint8_t> bytes = serializeSave(GameState::newGame(1));
    bytes.resize(6); // magic + version + a couple bytes of world seed, no more
    GameState loaded(0);
    CHECK(!deserializeSave(bytes, &loaded));
}

int main() {
    printf("test_save:\n");
    RUN(test_round_trip_preserves_everything);
    RUN(test_unmodified_chunk_regenerates_identically);
    RUN(test_bad_magic_rejected);
    RUN(test_version_mismatch_rejected);
    RUN(test_truncated_buffer_rejected);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
