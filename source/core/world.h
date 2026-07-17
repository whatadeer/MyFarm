#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/chunk.h"

namespace core {

// In-memory cache of every chunk visited this session. No eviction-by-
// distance in Milestone 1 - a session realistically touches at most a few
// hundred chunks, well under a megabyte of tile data, trivial for 3DS RAM.
// That's a deliberate simplification, not an oversight: only chunks the
// player has actually modified get persisted to the save file (see
// core/save.cpp), so the save stays small regardless of how far they
// wander, and eviction only matters for memory - a pure optimization to
// add later if a real session ever shows it's needed.
class ChunkStore {
public:
    explicit ChunkStore(uint32_t worldSeed) : worldSeed_(worldSeed) {}

    // Returns the chunk at `coord`, generating it deterministically on
    // first access. References stay valid for the life of the ChunkStore -
    // std::unordered_map guarantees existing element references survive
    // rehashing (only iterators are invalidated).
    Chunk& getOrGenerate(ChunkCoord coord);

    Tile& tileAt(int32_t worldX, int32_t worldY);

    // Call after mutating a tile obtained via tileAt() so it gets persisted.
    void markDirty(int32_t worldX, int32_t worldY);

    // Chunks the player has actually modified, for save().
    std::vector<const Chunk*> dirtyChunks() const;

    // Inserts/overwrites a chunk (used when loading a save's persisted
    // dirty-chunk list) and marks it dirty - it's already known modified,
    // or it wouldn't have been in the save file.
    void setChunk(const Chunk& chunk);

    size_t loadedCount() const { return chunks_.size(); }

private:
    uint32_t worldSeed_;
    std::unordered_map<uint64_t, Chunk> chunks_;

    static uint64_t key(ChunkCoord coord);
};

} // namespace core
