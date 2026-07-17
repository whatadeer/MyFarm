#pragma once

#include <cstdint>

#include "core/tile.h"

namespace core {

constexpr int kChunkSize = 16;

struct ChunkCoord {
    int32_t cx = 0;
    int32_t cy = 0;

    bool operator==(const ChunkCoord& o) const { return cx == o.cx && cy == o.cy; }
};

struct Chunk {
    ChunkCoord coord;
    Tile tiles[kChunkSize * kChunkSize];
    bool dirty = false;

    Tile& at(int localX, int localY) { return tiles[localY * kChunkSize + localX]; }
    const Tile& at(int localX, int localY) const { return tiles[localY * kChunkSize + localX]; }
};

// Deterministic world generation: same (worldSeed, coord) always produces a
// byte-identical chunk, so an unmodified chunk never needs to be saved -
// only ever regenerated on demand as the player walks into it. Mostly
// Grass, occasional Water (whole 8x8-tile regions, so ponds/lakes read as
// blobs rather than single-tile speckle), sparse decorative Trees/Rocks
// scattered on Grass (Milestone 1: decorative only, not yet
// choppable/mineable - see the plan's Milestone 2 section).
Chunk generateChunk(uint32_t worldSeed, ChunkCoord coord);

// World tile coord -> which chunk/local-tile it falls in. Uses floor
// division (not C++'s truncating `/`) so negative coordinates - the world
// extends in every direction from spawn - work correctly.
ChunkCoord worldToChunk(int32_t worldX, int32_t worldY);
void worldToLocal(int32_t worldX, int32_t worldY, int* localX, int* localY);

// Purely-visual region flavor: which biome's tree/stump art a tile uses.
// Deterministic from (seed, coords) on a coarse 64x64-tile grid - never
// stored and never affects balance (wood is wood), just exploration
// variety. Meadow is most common; birch/cherry-blossom/pine regions
// appear as the player wanders.
enum class Biome : uint8_t {
    Meadow = 0,
    Birch = 1,
    Cherry = 2,
    Pine = 3,
    Snow = 4, // the far north: snow ground, walkable ice, snowy trees
};
Biome biomeAt(uint32_t worldSeed, int32_t worldX, int32_t worldY);

// Hedge mazes: rare walled gardens (binary-tree mazes, decidable per
// tile). Returns 0 outside any maze, 1 hedge wall, 2 maze path.
int mazeAt(uint32_t worldSeed, int32_t worldX, int32_t worldY);

// Elevation: rounded plateau blobs (premium hill tileset). Pure function
// of (seed, coords) like biomeAt - never stored. Raised ground draws a
// cliff rim and blocks walking across the edge except down stair ramps
// (rampAt, meaningful on a raised rim tile whose south neighbor is low).
// Spawn's immediate surroundings and the Snowlands stay flat.
bool elevAt(uint32_t worldSeed, int32_t worldX, int32_t worldY);
bool rampAt(uint32_t worldSeed, int32_t worldX, int32_t worldY);

} // namespace core
