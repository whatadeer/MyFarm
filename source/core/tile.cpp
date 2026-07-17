#include "core/tile.h"

#include "core/balance.h"
#include "core/growth_timer.h"

namespace core {

namespace {

const NodeBalance* balanceFor(Decoration deco) {
    switch (deco) {
        case Decoration::Tree: return &kTreeBalance;
        case Decoration::Rock: return &kRockBalance;
        case Decoration::Bush: return &kBushBalance;
        case Decoration::Mushroom: return &kMushroomBalance;
        case Decoration::WildPumpkin:
        case Decoration::WildSunflower: return &kWildPatchBalance;
        default: return nullptr;
    }
}

int clampTier(uint8_t tier) {
    return tier >= kNodeTiers ? kNodeTiers - 1 : tier;
}

} // namespace

bool blocksMovement(const Tile& tile, bool waterFrozen, int64_t now) {
    bool waterBlocks = tile.terrain == Terrain::Water && !waterFrozen;
    if (tile.placed == Placed::Bridge) return false; // walkable water
    if (tile.placed == Placed::Gate) return false;      // walk-through fence opening (left leaf)
    if (tile.placed == Placed::GateRight) return false; // walk-through fence opening (right leaf)
    if (tile.placed == Placed::Door) return false;   // walk-through wall opening
    if (tile.placed == Placed::Rug || tile.placed == Placed::RugLong) {
        return waterBlocks || tile.terrain == Terrain::Hole;
    }
    if (tile.placed != Placed::None) return true;
    if (tile.decoration != Decoration::None) {
        // Hedge-maze walls are always solid (no gather, no regrow).
        if (tile.decoration == Decoration::Hedge) return true;
        // Loose pebbles are underfoot-small - never block.
        if (tile.decoration == Decoration::Pebble) {
            return waterBlocks || tile.terrain == Terrain::Hole;
        }
        // A chopped tree / picked patch is just a stump or sprout while
        // it regrows - walk right over it (when the caller has a clock).
        if (now == 0) return true;
        return nodeReady(tile, now);
    }
    return waterBlocks || tile.terrain == Terrain::Hole;
}

bool canPlace(const Tile& tile) {
    // Floor counts as buildable ground so walls/furniture can go on it.
    return (tile.terrain == Terrain::Grass || tile.terrain == Terrain::Dirt ||
            tile.terrain == Terrain::Floor) &&
           !tile.tilled && !tile.hasCrop &&
           tile.decoration == Decoration::None && tile.placed == Placed::None;
}

bool canPlaceBridge(const Tile& tile) {
    return tile.terrain == Terrain::Water && tile.placed == Placed::None;
}

ShovelResult useShovel(Tile& tile) {
    // Demolish a placed object first (bridge included - shovel it away to
    // get the water back). The scene layer refuses this for non-empty
    // chests before calling.
    if (tile.placed != Placed::None) {
        tile.placed = Placed::None;
        return ShovelResult::Removed;
    }
    if (tile.tilled) {
        if (tile.hasCrop) return ShovelResult::Blocked;
        tile.tilled = false;
        tile.terrain = Terrain::Dirt;
        return ShovelResult::Untilled;
    }
    if (tile.terrain == Terrain::Hole) {
        tile.terrain = Terrain::Dirt;
        return ShovelResult::Filled;
    }
    if ((tile.terrain == Terrain::Grass || tile.terrain == Terrain::Dirt ||
         isPathTerrain(tile.terrain) || tile.terrain == Terrain::Floor) &&
        tile.decoration == Decoration::None && !tile.hasCrop) {
        // Shoveling a path/rail/floor just reverts it to dirt; grass/dirt
        // digs a hole.
        if (isPathTerrain(tile.terrain) || tile.terrain == Terrain::Floor) {
            tile.terrain = Terrain::Dirt;
            return ShovelResult::Untilled;
        }
        tile.terrain = Terrain::Hole;
        return ShovelResult::Dug;
    }
    return ShovelResult::Blocked;
}

bool pourWater(Tile& tile) {
    if (tile.terrain != Terrain::Hole || tile.placed != Placed::None) return false;
    tile.terrain = Terrain::Water;
    return true;
}

bool plantSapling(Tile& tile, int64_t now) {
    if (tile.terrain != Terrain::Hole || tile.placed != Placed::None ||
        tile.decoration != Decoration::None) {
        return false;
    }
    tile.terrain = Terrain::Dirt;
    tile.decoration = Decoration::Tree;
    tile.decoTier = 0;
    tile.depleted = true; // "growing" reuses the depleted/respawn machinery
    tile.timestamp = now;
    return true;
}

bool nodeReady(const Tile& tile, int64_t now) {
    const NodeBalance* bal = balanceFor(tile.decoration);
    if (!bal) return false;
    if (!tile.depleted) return true;
    return elapsedAtLeast(tile.timestamp, now, bal->respawnSec[clampTier(tile.decoTier)]);
}

GatherResult gatherNode(Tile& tile, Decoration expected, int skillLevel, int levelReq,
                        int64_t now) {
    const NodeBalance* bal = balanceFor(tile.decoration);
    if (!bal || tile.decoration != expected) return GatherResult::NotANode;
    if (!nodeReady(tile, now)) return GatherResult::Regrowing;
    if (skillLevel < levelReq) return GatherResult::LevelTooLow;

    tile.depleted = true;
    tile.timestamp = now;
    return GatherResult::Ok;
}

} // namespace core
