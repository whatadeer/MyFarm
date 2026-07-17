#include "core/game_state.h"

#include "core/item_db.h"
#include "core/tile.h"

namespace core {

ChestData* GameState::chestAt(int32_t x, int32_t y) {
    for (ChestData& c : chests) {
        if (c.x == x && c.y == y) return &c;
    }
    return nullptr;
}

HiveData* GameState::hiveAt(int32_t x, int32_t y) {
    for (HiveData& h : hives) {
        if (h.x == x && h.y == y) return &h;
    }
    return nullptr;
}

GroundItem* GameState::groundItemAt(int32_t x, int32_t y) {
    for (GroundItem& g : groundItems) {
        if (g.x == x && g.y == y) return &g;
    }
    return nullptr;
}

InteriorData* GameState::interiorAt(int32_t bx, int32_t by) {
    for (InteriorData& i : interiors) {
        if (i.bx == bx && i.by == by) return &i;
    }
    return nullptr;
}

GameState GameState::newGame(uint32_t seed) {
    GameState state(seed);
    // Spawn on the first open tile spiralling out from the origin -
    // worldgen may have put a tree/rock/pond right at (0,0), and spawning
    // inside a blocker means being stuck forever.
    state.playerPos = {0.5f, 0.5f};
    for (int r = 0; r < 10; r++) {
        bool found = false;
        for (int dy = -r; dy <= r && !found; dy++) {
            for (int dx = -r; dx <= r && !found; dx++) {
                if ((dx > -r && dx < r) && (dy > -r && dy < r)) continue; // ring only
                if (!blocksMovement(state.world.tileAt(dx, dy))) {
                    state.playerPos = {static_cast<float>(dx) + 0.5f,
                                       static_cast<float>(dy) + 0.5f};
                    found = true;
                }
            }
        }
        if (found) break;
    }
    // Just an Axe. Chop wood, pile it into a Workbench, and craft the
    // rest of the toolkit there - the game's first quest, implicitly.
    state.toolBelt.add(kItemAxe);
    return state;
}

} // namespace core
