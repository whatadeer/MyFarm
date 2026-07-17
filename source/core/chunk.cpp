#include "core/chunk.h"

#include <algorithm>
#include <cstring>

namespace core {

namespace {

int32_t floorDiv(int32_t a, int32_t b) {
    int32_t q = a / b;
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q--;
    return q;
}

int32_t floorMod(int32_t a, int32_t b) {
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

// Deterministic integer hash (squirrel-noise style: multiply/xor-shift
// rounds for avalanche) - no floating point, no external noise library,
// just enough so terrain doesn't look obviously repetitive.
uint32_t hashCoords(uint32_t seed, int32_t x, int32_t y, uint32_t salt) {
    uint32_t h = seed ^ 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h ^= static_cast<uint32_t>(y) * 0xC2B2AE35u;
    h ^= salt * 0x27D4EB2Fu;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

constexpr uint32_t kSaltWater = 1;
constexpr uint32_t kSaltDecoration = 2;
constexpr uint32_t kSaltTier = 3;

// Organic lakes: a union of jittered circles seeded on a coarse lattice
// (the same trick as elevAt's plateaus). The old scheme filled whole
// 8x8 regions, which made every pond a perfect square. A small dry zone
// keeps spawn's first screen workable.
bool waterAt(uint32_t worldSeed, int32_t wx, int32_t wy) {
    if (wx > -10 && wx < 10 && wy > -10 && wy < 10) return false;
    constexpr int32_t kLattice = 20;
    int32_t gx = floorDiv(wx, kLattice);
    int32_t gy = floorDiv(wy, kLattice);
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            uint32_t h = hashCoords(worldSeed, gx + dx, gy + dy, kSaltWater);
            if (h % 100 >= 22) continue; // ~22% of cells host a lake
            int32_t cx = (gx + dx) * kLattice + 4 + static_cast<int32_t>((h >> 8) % 12);
            int32_t cy = (gy + dy) * kLattice + 4 + static_cast<int32_t>((h >> 16) % 12);
            int32_t r = 4 + static_cast<int32_t>((h >> 24) % 5); // 4..8 tiles
            int64_t ddx = wx - cx, ddy = wy - cy;
            if (ddx * ddx + ddy * ddy <= static_cast<int64_t>(r) * r) return true;
        }
    }
    return false;
}

// Distance bands (squared, in tiles from spawn) for node tiers: near spawn
// everything is tier 0; mid-range mixes in tier 1; far out all three tiers
// appear. Higher tiers need higher skill levels (balance.h) and are the
// procedurally recolored art variants - exploring outward is how you find
// them.
constexpr int64_t kTier1BandSq = 48 * 48;
constexpr int64_t kTier2BandSq = 128 * 128;
// Ancient giants: trees only - a rare tier 3 out past everything else.
constexpr int64_t kAncientBandSq = 220 * 220;
constexpr uint32_t kSaltAncient = 17;

uint8_t rollTier(uint32_t worldSeed, int32_t wx, int32_t wy) {
    int64_t d2 = static_cast<int64_t>(wx) * wx + static_cast<int64_t>(wy) * wy;
    if (d2 < kTier1BandSq) return 0;
    uint32_t roll = hashCoords(worldSeed, wx, wy, kSaltTier) % 100;
    if (d2 < kTier2BandSq) {
        return roll < 40 ? 1 : 0;
    }
    if (roll < 25) return 2;
    if (roll < 70) return 1;
    return 0;
}

// Wild plant patches: organic CLUMPS out in the wild (jittered circles
// on a sparse lattice, per-tile thinning so they read scattered, not
// solid). Four kinds: pumpkin clumps, sunflower stands, berry-bush
// thickets, and mushroom FAIRY RINGS (hollow center, dense rim). No
// patches in the Snowlands.
Decoration wildPatchAt(uint32_t worldSeed, int32_t wx, int32_t wy) {
    constexpr int32_t kLattice = 36;
    int32_t gx = floorDiv(wx, kLattice);
    int32_t gy = floorDiv(wy, kLattice);
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            uint32_t h = hashCoords(worldSeed, gx + dx, gy + dy, 8);
            if (h % 100 >= 20) continue;
            int32_t cx = (gx + dx) * kLattice + 8 + static_cast<int32_t>((h >> 8) % 20);
            int32_t cy = (gy + dy) * kLattice + 8 + static_cast<int32_t>((h >> 16) % 20);
            int32_t r = 2 + static_cast<int32_t>((h >> 26) % 3); // 2..4 tiles
            int64_t ddx = wx - cx, ddy = wy - cy;
            int64_t d2 = ddx * ddx + ddy * ddy;
            int64_t r2 = static_cast<int64_t>(r) * r;
            if (d2 > r2) continue;
            if (biomeAt(worldSeed, wx, wy) == Biome::Snow) continue;
            uint32_t kind = (h >> 2) % 4;
            uint32_t local = hashCoords(worldSeed, wx, wy, 9) % 100;
            if (kind == 2) {
                // Fairy ring: hollow center, dense mushroom rim.
                if (d2 * 2 < r2) continue;
                if (local >= 70) continue;
                return Decoration::Mushroom;
            }
            // Blob kinds thin from a per-patch base density (30-60%) all
            // the way off toward the fringe - real gaps inside,
            // stragglers outside.
            uint32_t fill = 30 + ((h >> 5) % 31);
            uint32_t threshold =
                fill - static_cast<uint32_t>(fill * d2 / (r2 > 0 ? r2 : 1));
            if (local >= threshold) continue;
            return kind == 0   ? Decoration::WildPumpkin
                   : kind == 1 ? Decoration::WildSunflower
                               : Decoration::Bush;
        }
    }
    return Decoration::None;
}

} // namespace

// Hedge mazes: rare walled gardens out in the meadow. A binary-tree
// maze is decidable per tile with NO global state: every odd-odd cell
// carves one passage (north or east) by its own hash bit; that yields a
// perfect maze, and any tile can tell hedge-or-path purely from its
// neighboring cells' choices.
//
// The whole thing is mirrored left-right around the center column: every
// lookup folds its column through fold(lx) = min(lx, kMazeW-1-lx) before
// hashing or applying the boundary rules below, so a connector and its
// mirror image always evaluate the identical call and agree. Folding
// replaces the original single "top-right" sink with a mirrored pair of
// sinks that converge on the axis instead.
//
// A central room (an open colonnade - hedge only at its four corner
// posts) is carved through the middle rows. Its border is intentionally
// left open rather than walled: the room removes a block of interior
// cells from what would otherwise be one connected spanning tree, and a
// fully-walled room would risk sealing off whichever corridor branches
// only ever touched it at now-hedged points. Leaving every non-corner
// edge open means any branch that reaches the room always re-merges
// through it, so the maze stays fully connected. The wall slots diagonal
// to the corner posts are forced open too - the renderer's cardinal-mask
// blob has no tile for two hedges meeting only at a diagonal.
//
// Returns 0 outside, 1 hedge wall, 2 path.
int mazeAt(uint32_t worldSeed, int32_t wx, int32_t wy) {
    constexpr int32_t kLattice = 96;
    constexpr int32_t kMazeW = 17, kMazeH = 13; // odd, border included
    constexpr int32_t kRoomX0 = 5, kRoomX1 = 11; // symmetric about the axis
    constexpr int32_t kRoomY0 = 4, kRoomY1 = 8;
    int32_t gx = floorDiv(wx, kLattice);
    int32_t gy = floorDiv(wy, kLattice);
    for (int32_t dy = -1; dy <= 0; dy++) {
        for (int32_t dx = -1; dx <= 0; dx++) {
            uint32_t h = hashCoords(worldSeed, gx + dx, gy + dy, 10);
            if (h % 100 >= 7) continue; // ~7% of 96-tile cells host one
            int32_t ox = (gx + dx) * kLattice + 20 +
                         static_cast<int32_t>((h >> 8) % (kLattice - kMazeW - 40));
            int32_t oy = (gy + dy) * kLattice + 20 +
                         static_cast<int32_t>((h >> 16) % (kLattice - kMazeH - 40));
            int32_t lx = wx - ox, ly = wy - oy;
            if (lx < 0 || ly < 0 || lx >= kMazeW || ly >= kMazeH) continue;
            // Not near spawn, not in the frozen north.
            if (ox > -60 && ox < 60 && oy > -60 && oy < 60) continue;
            if (biomeAt(worldSeed, ox, oy) == Biome::Snow) continue;
            // Twin entrances in the south border, mirrored about the axis.
            if (ly == kMazeH - 1 && (lx == kMazeW / 2 - 1 || lx == kMazeW / 2 + 1)) return 2;
            if (lx == 0 || ly == 0 || lx == kMazeW - 1 || ly == kMazeH - 1) return 1;
            // Central room: open colonnade, hedge only at the corner posts.
            if (lx >= kRoomX0 && lx <= kRoomX1 && ly >= kRoomY0 && ly <= kRoomY1) {
                bool corner = (lx == kRoomX0 || lx == kRoomX1) && (ly == kRoomY0 || ly == kRoomY1);
                return corner ? 1 : 2;
            }
            // Kissing-corner guard: each corner post sits diagonal to one
            // ordinary wall slot with both shared cardinals open (the
            // room border and an odd-odd cell). The 16-tile hedge blob
            // has no art for hedges that touch only at a diagonal, so
            // force those four slots open. Opening a wall merely merges
            // two corridors (adds a loop) - it can never disconnect.
            if ((lx == kRoomX0 + 1 || lx == kRoomX1 - 1) &&
                (ly == kRoomY0 - 1 || ly == kRoomY1 + 1))
                return 2;
            bool oddX = (lx % 2) == 1, oddY = (ly % 2) == 1;
            if (oddX && oddY) return 2;  // room cell
            if (!oddX && !oddY) return 1; // pillar
            // Binary-tree carve, folded left-right about the axis: cells
            // adjacent to the axis carve toward it (mirroring the old
            // right-column rule), top-row cells carve east, everyone
            // else flips a hash coin (the two axis-adjacent top cells
            // carve nothing; their neighbors reach them).
            constexpr int32_t kAxisCol = kMazeW / 2 - 1;
            auto carvesNorth = [&](int32_t cx2, int32_t cy2) {
                bool topRow = cy2 == 1, atAxis = cx2 == kAxisCol;
                if (topRow) return false;
                if (atAxis) return true;
                return (hashCoords(worldSeed, ox + cx2, oy + cy2, 11) & 1) != 0;
            };
            auto carvesEast = [&](int32_t cx2, int32_t cy2) {
                bool topRow = cy2 == 1, atAxis = cx2 == kAxisCol;
                if (atAxis) return false;
                if (topRow) return true;
                return (hashCoords(worldSeed, ox + cx2, oy + cy2, 11) & 1) == 0;
            };
            int32_t fx = std::min(lx, kMazeW - 1 - lx);
            if (oddX) return carvesNorth(fx, ly + 1) ? 2 : 1;  // wall above cell
            return carvesEast(fx - 1, ly) ? 2 : 1;              // wall right of cell
        }
    }
    return 0;
}

namespace {

// What a tile WOULD generate, as a pure function - callable for any tile
// (including neighbors in other chunks) so clearance rules below stay
// deterministic and chunk-border-safe.
void rollDeco(uint32_t worldSeed, int32_t wx, int32_t wy, Decoration* deco, uint8_t* tier) {
    *deco = Decoration::None;
    *tier = 0;
    int mz = mazeAt(worldSeed, wx, wy);
    if (mz == 1) {
        *deco = Decoration::Hedge;
        return;
    }
    if (mz == 2) {
        // Maze paths grow their own sparse rewards - mushrooms and berry
        // bushes worth braving the dead ends for.
        uint32_t r = hashCoords(worldSeed, wx, wy, 12) % 100;
        if (r < 5) {
            *deco = Decoration::Mushroom;
            *tier = rollTier(worldSeed, wx, wy);
        } else if (r < 10) {
            *deco = Decoration::Bush;
            *tier = rollTier(worldSeed, wx, wy);
        }
        return;
    }
    if (waterAt(worldSeed, wx, wy)) return; // water
    Decoration patch = wildPatchAt(worldSeed, wx, wy);
    if (patch != Decoration::None) {
        *deco = patch;
        // Thicket bushes and ring mushrooms keep the normal distance-
        // based tier roll (better finds farther out, as usual).
        if (patch == Decoration::Bush || patch == Decoration::Mushroom) {
            *tier = rollTier(worldSeed, wx, wy);
        }
        return;
    }
    uint32_t decoRoll = hashCoords(worldSeed, wx, wy, kSaltDecoration) % 1000;
    if (decoRoll < 45) {
        *deco = Decoration::Tree;
    } else if (decoRoll < 75) {
        *deco = Decoration::Rock;
    } else if (decoRoll < 105) {
        *deco = Decoration::Bush;
    } else if (decoRoll < 120) {
        *deco = Decoration::Mushroom;
    } else if (decoRoll < 135) {
        // Loose pebbles: pick-up stones, no tier, no blocking.
        *deco = Decoration::Pebble;
        return;
    } else if (decoRoll < 148) {
        // Deadfall: pick-up wood, no tier, no blocking. Extends the roll
        // table past the old bands so existing worlds only GAIN logs on
        // tiles that previously rolled nothing.
        *deco = Decoration::FallenLog;
        return;
    }
    if (*deco != Decoration::None) *tier = rollTier(worldSeed, wx, wy);
    // The far field grows ancient giants: past the ancient band, one tree
    // in ~40 upgrades to the 3x3-tile tier 3.
    if (*deco == Decoration::Tree &&
        static_cast<int64_t>(wx) * wx + static_cast<int64_t>(wy) * wy >= kAncientBandSq &&
        hashCoords(worldSeed, wx, wy, kSaltAncient) % 40 == 0) {
        *tier = 3;
    }
}

// Big boulders (rock tiers 1-2) draw 2-3 tiles wide, and ancient trees
// (tree tier 3) draw a full 3x3, so they claim their 8 neighbors:
// anything else that would spawn beside one is suppressed. Between two
// adjacent bigs, the lower (y, x) one wins.
bool isBigDeco(Decoration deco, uint8_t tier) {
    return (deco == Decoration::Rock && tier >= 1) ||
           (deco == Decoration::Tree && tier >= 3);
}

bool suppressedByNeighbor(uint32_t worldSeed, int32_t wx, int32_t wy,
                          Decoration selfDeco, uint8_t selfTier) {
    bool selfBig = isBigDeco(selfDeco, selfTier);
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            Decoration nd;
            uint8_t nt;
            rollDeco(worldSeed, wx + dx, wy + dy, &nd, &nt);
            if (!isBigDeco(nd, nt)) continue;
            if (!selfBig) return true;
            // Two bigs side by side: earlier scan order wins.
            if (dy < 0 || (dy == 0 && dx < 0)) return true;
        }
    }
    return false;
}

} // namespace

Biome biomeAt(uint32_t worldSeed, int32_t worldX, int32_t worldY) {
    constexpr int32_t kBiomeRegion = 64;
    int32_t bx = floorDiv(worldX, kBiomeRegion);
    int32_t by = floorDiv(worldY, kBiomeRegion);
    // The far north is permanent winter - travel up (negative Y) past a
    // jagged, per-column treeline and everything is Snowlands: snow
    // ground, walkable ice ponds, snow-covered trees.
    int32_t snowLine = -176 - static_cast<int32_t>(hashCoords(worldSeed, bx, 0, 5) % 64);
    if (worldY <= snowLine) return Biome::Snow;
    // Spawn's region is always Meadow so the tutorial-ish first screen uses
    // the classic art.
    if (bx == 0 && by == 0) return Biome::Meadow;
    uint32_t roll = hashCoords(worldSeed, bx, by, 4) % 10;
    if (roll < 4) return Biome::Meadow;
    if (roll < 6) return Biome::Birch;
    if (roll < 8) return Biome::Cherry;
    return Biome::Pine;
}

bool elevAt(uint32_t worldSeed, int32_t worldX, int32_t worldY) {
    // Keep the tutorial-ish spawn screen flat, and the Snowlands too (the
    // hill art is green Meadow grass). Lakes carve through hills - a
    // shoreline never carries a cliff rim (and plateau shores stay
    // unclimbable, since leaving the water onto raised ground is still a
    // cliff crossing).
    if (worldX > -14 && worldX < 14 && worldY > -14 && worldY < 14) return false;
    if (waterAt(worldSeed, worldX, worldY)) return false;
    if (biomeAt(worldSeed, worldX, worldY) == Biome::Snow) return false;
    if (mazeAt(worldSeed, worldX, worldY) != 0) return false; // no cliffs in mazes
    // Plateau blobs: candidate center points on a coarse 24-tile lattice,
    // each present ~30% of the time with a jittered center and its own
    // radius (5-9 tiles). A tile is raised if it falls inside any nearby
    // center's circle - rounded, varied hills with no stored state.
    constexpr int32_t kLattice = 24;
    int32_t gx = floorDiv(worldX, kLattice);
    int32_t gy = floorDiv(worldY, kLattice);
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            uint32_t h = hashCoords(worldSeed, gx + dx, gy + dy, 6);
            if (h % 100 >= 30) continue;
            int32_t cx = (gx + dx) * kLattice + 6 + static_cast<int32_t>((h >> 8) % 12);
            int32_t cy = (gy + dy) * kLattice + 6 + static_cast<int32_t>((h >> 16) % 12);
            int32_t r = 5 + static_cast<int32_t>((h >> 24) % 5);
            int64_t ddx = worldX - cx, ddy = worldY - cy;
            if (ddx * ddx + ddy * ddy <= static_cast<int64_t>(r) * r) return true;
        }
    }
    return false;
}

bool rampAt(uint32_t worldSeed, int32_t worldX, int32_t worldY) {
    return hashCoords(worldSeed, worldX, worldY, 7) % 4 == 0;
}

ChunkCoord worldToChunk(int32_t worldX, int32_t worldY) {
    return {floorDiv(worldX, kChunkSize), floorDiv(worldY, kChunkSize)};
}

void worldToLocal(int32_t worldX, int32_t worldY, int* localX, int* localY) {
    *localX = floorMod(worldX, kChunkSize);
    *localY = floorMod(worldY, kChunkSize);
}

Chunk generateChunk(uint32_t worldSeed, ChunkCoord coord) {
    Chunk chunk;
    // Zero the whole tile block first so generation is byte-deterministic
    // (struct padding included) - tests memcmp generated chunks, and the
    // save file round-trips them. void* cast: Tile's default member
    // initializers are all zero, so raw-zeroing is equivalent and the
    // -Wclass-memaccess warning is noise here.
    std::memset(static_cast<void*>(chunk.tiles), 0, sizeof(chunk.tiles));
    chunk.coord = coord;
    chunk.dirty = false;

    for (int ly = 0; ly < kChunkSize; ly++) {
        for (int lx = 0; lx < kChunkSize; lx++) {
            int32_t wx = coord.cx * kChunkSize + lx;
            int32_t wy = coord.cy * kChunkSize + ly;

            Tile& tile = chunk.at(lx, ly);
            // Mazes claim their footprint before water gets a say - a
            // pond punching through a hedge wall would break the puzzle.
            if (mazeAt(worldSeed, wx, wy) == 0 && waterAt(worldSeed, wx, wy)) {
                tile.terrain = Terrain::Water;
                continue;
            }

            tile.terrain = Terrain::Grass;
            // Per-mille rolls: 4.5% tree, 3% rock, 3% bush, 1.5% mushroom
            // - then big-boulder clearance (see suppressedByNeighbor).
            // Hedge walls are never suppressed (a big rock outside the
            // maze must not knock holes in it).
            Decoration deco;
            uint8_t tier;
            rollDeco(worldSeed, wx, wy, &deco, &tier);
            if (deco != Decoration::None &&
                (deco == Decoration::Hedge ||
                 !suppressedByNeighbor(worldSeed, wx, wy, deco, tier))) {
                tile.decoration = deco;
                tile.decoTier = tier;
            }
        }
    }

    return chunk;
}

} // namespace core
