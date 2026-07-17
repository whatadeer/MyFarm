// Host-native tests for source/core/world.h: the ChunkStore cache and its
// dirty-tracking (which drives what actually gets persisted to a save).
#include "core/world.h"
#include "minitest.h"

using namespace core;

static void test_get_or_generate_returns_same_instance() {
    ChunkStore store(1);
    Chunk& a = store.getOrGenerate(ChunkCoord{0, 0});
    Chunk& b = store.getOrGenerate(ChunkCoord{0, 0});
    CHECK(&a == &b);
}

static void test_tile_at_round_trips_through_chunk() {
    ChunkStore store(1);
    Tile& t = store.tileAt(5, 5);
    t.terrain = Terrain::Dirt;
    Tile& t2 = store.tileAt(5, 5);
    CHECK(&t == &t2);
    CHECK(t2.terrain == Terrain::Dirt);
}

static void test_new_chunks_start_clean() {
    ChunkStore store(1);
    store.getOrGenerate(ChunkCoord{7, 7});
    CHECK(store.dirtyChunks().empty());
}

static void test_mark_dirty_only_affects_touched_chunk() {
    ChunkStore store(1);
    store.getOrGenerate(ChunkCoord{0, 0});
    store.getOrGenerate(ChunkCoord{1, 0});
    store.markDirty(5, 5); // inside chunk (0,0)

    auto dirty = store.dirtyChunks();
    CHECK(dirty.size() == 1);
    if (dirty.size() == 1) {
        CHECK(dirty[0]->coord.cx == 0 && dirty[0]->coord.cy == 0);
    }
}

static void test_set_chunk_overwrites_and_marks_dirty() {
    ChunkStore store(1);
    Chunk custom;
    custom.coord = ChunkCoord{2, 2};
    custom.tiles[0].terrain = Terrain::Water;
    store.setChunk(custom);

    Chunk& loaded = store.getOrGenerate(ChunkCoord{2, 2});
    CHECK(loaded.tiles[0].terrain == Terrain::Water);
    CHECK(loaded.dirty);
}

int main() {
    printf("test_world:\n");
    RUN(test_get_or_generate_returns_same_instance);
    RUN(test_tile_at_round_trips_through_chunk);
    RUN(test_new_chunks_start_clean);
    RUN(test_mark_dirty_only_affects_touched_chunk);
    RUN(test_set_chunk_overwrites_and_marks_dirty);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
