// Host-native tests for source/core/chunk.h: deterministic world
// generation and world<->chunk/local coordinate math.
#include <cstring>

#include "core/chunk.h"
#include "minitest.h"

using namespace core;

static void test_deterministic_generation() {
    Chunk a = generateChunk(42, ChunkCoord{3, -5});
    Chunk b = generateChunk(42, ChunkCoord{3, -5});
    CHECK(std::memcmp(a.tiles, b.tiles, sizeof(a.tiles)) == 0);
}

static void test_different_seed_differs() {
    Chunk a = generateChunk(1, ChunkCoord{0, 0});
    Chunk b = generateChunk(2, ChunkCoord{0, 0});
    CHECK(std::memcmp(a.tiles, b.tiles, sizeof(a.tiles)) != 0);
}

static void test_different_coord_differs() {
    Chunk a = generateChunk(42, ChunkCoord{0, 0});
    Chunk b = generateChunk(42, ChunkCoord{1, 0});
    CHECK(std::memcmp(a.tiles, b.tiles, sizeof(a.tiles)) != 0);
}

static void test_density_is_sane() {
    // Sample a wide swath of chunks and check nothing is degenerate (e.g.
    // "everything is water" or "nothing ever generates a tree").
    int water = 0, grass = 0, tree = 0, rock = 0, bush = 0, mushroom = 0, total = 0;
    for (int32_t cx = 0; cx < 20; cx++) {
        for (int32_t cy = 0; cy < 20; cy++) {
            Chunk c = generateChunk(1234, ChunkCoord{cx, cy});
            for (const Tile& t : c.tiles) {
                total++;
                if (t.terrain == Terrain::Water) {
                    water++;
                } else {
                    grass++;
                    if (t.decoration == Decoration::Tree) tree++;
                    if (t.decoration == Decoration::Rock) rock++;
                    if (t.decoration == Decoration::Bush) bush++;
                    if (t.decoration == Decoration::Mushroom) mushroom++;
                }
            }
        }
    }
    CHECK(total == 20 * 20 * kChunkSize * kChunkSize);
    CHECK(water > 0 && water < total);
    CHECK(grass > water); // grass should dominate given ~6% water regions
    CHECK(tree > 0);
    CHECK(rock > 0);
    CHECK(bush > 0);
    CHECK(mushroom > 0);
}

static void test_tiers_scale_with_distance_from_spawn() {
    // Near spawn (within the 48-tile band) every node is tier 0; far out
    // (past 128 tiles) tiers 1 and 2 both appear.
    int nearHigh = 0, farT1 = 0, farT2 = 0;
    for (int32_t cx = -2; cx < 2; cx++) {
        for (int32_t cy = -2; cy < 2; cy++) {
            Chunk c = generateChunk(99, ChunkCoord{cx, cy});
            for (int ly = 0; ly < kChunkSize; ly++) {
                for (int lx = 0; lx < kChunkSize; lx++) {
                    int32_t wx = cx * kChunkSize + lx;
                    int32_t wy = cy * kChunkSize + ly;
                    const Tile& t = c.at(lx, ly);
                    // Only the guaranteed-inner band: chunk corners can poke
                    // past 48 tiles diagonally, so filter by true distance.
                    if (static_cast<int64_t>(wx) * wx + static_cast<int64_t>(wy) * wy >= 48 * 48) continue;
                    if (t.decoration != Decoration::None && t.decoTier > 0) nearHigh++;
                }
            }
        }
    }
    for (int32_t cx = 12; cx < 18; cx++) { // world x 192.. - beyond the 128-tile band
        for (int32_t cy = 12; cy < 18; cy++) {
            Chunk c = generateChunk(99, ChunkCoord{cx, cy});
            for (const Tile& t : c.tiles) {
                if (t.decoration == Decoration::None) continue;
                if (t.decoTier == 1) farT1++;
                if (t.decoTier == 2) farT2++;
            }
        }
    }
    CHECK(nearHigh == 0);
    CHECK(farT1 > 0);
    CHECK(farT2 > 0);
}

static void test_biomes_deterministic_with_snow_up_north() {
    // Same inputs, same biome.
    CHECK(biomeAt(9, 500, 300) == biomeAt(9, 500, 300));
    // Spawn's region is always Meadow.
    CHECK(biomeAt(12345, 0, 0) == Biome::Meadow);
    CHECK(biomeAt(999, 30, 20) == Biome::Meadow);
    for (int32_t x = -300; x <= 300; x += 37) {
        // Far enough north is always Snowlands (the jagged snow line
        // starts at worldY -176 and reaches at most -240)...
        CHECK(biomeAt(77, x, -240) == Biome::Snow);
        CHECK(biomeAt(77, x, -400) == Biome::Snow);
        // ...and the south never is.
        CHECK(biomeAt(77, x, 200) != Biome::Snow);
    }
}

static void test_world_to_chunk_and_local_positive() {
    ChunkCoord c = worldToChunk(20, 33);
    CHECK(c.cx == 1 && c.cy == 2);
    int lx, ly;
    worldToLocal(20, 33, &lx, &ly);
    CHECK(lx == 4 && ly == 1);
}

static void test_world_to_chunk_and_local_negative() {
    // Floor division, not truncation: -1 belongs to chunk -1 (local 15),
    // not chunk 0.
    ChunkCoord c = worldToChunk(-1, -16);
    CHECK(c.cx == -1 && c.cy == -1);
    int lx, ly;
    worldToLocal(-1, -16, &lx, &ly);
    CHECK(lx == 15 && ly == 0);
}

int main() {
    printf("test_chunk:\n");
    RUN(test_deterministic_generation);
    RUN(test_different_seed_differs);
    RUN(test_different_coord_differs);
    RUN(test_density_is_sane);
    RUN(test_tiers_scale_with_distance_from_spawn);
    RUN(test_biomes_deterministic_with_snow_up_north);
    RUN(test_world_to_chunk_and_local_positive);
    RUN(test_world_to_chunk_and_local_negative);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
