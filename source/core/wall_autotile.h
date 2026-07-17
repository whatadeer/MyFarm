#pragma once

#include <cstdint>

// Dungeon-wall autotiler: pure geometry -> role decisions, no atlas indices,
// no <3ds.h> - host-testable like the rest of core/. This is the round-4
// walk-behind rule, derived and verified in tools/wall_lab (the lab page is
// the reference implementation; tests/test_wall_autotile.cpp pins this port
// to the lab's output over every cell of every scenario the user reviewed).
//
// Mask bits: N=1 E=2 S=4 W=8; a SET bit means FLOOR on that side.
//
// The 2.5D scheme: every wall reads two tiles tall. A south-facing FACE keeps
// its tile; every other wall cell "peels" its N floor contact upward - the
// wall renders as if that floor weren't there, and the peeled rim lands on
// the floor cell above it as an OVERLAY the player walks behind.

namespace core::wallauto {

// Every tile the rule can name. The first 16 are the plain mask tiles and
// MUST stay contiguous in mask order - the rule indexes them by mask value.
enum class Role : uint8_t {
    W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, W12, W13, W14, W15,
    // Cap row (a cell directly above a south-facing face).
    R0, R1, R2, R2W, R3, R8, R8J, R9,
    // Crown / walk-behind ridge strips (overlay-only, over 1-tall runs).
    RcapL, RcapM, RcapR,
    // Face-row side junctions (a 1-tall run butts a 2-tall wall's side).
    Fcl, Fcr, FaceBoth,
    // Corner caps (cap-row cells curving toward the wall below's opening).
    Ccne, Ccnw, Ccne2, Ccnw2, CapBoth,
    // Corners (floor at a diagonal / a side run ending downward).
    Knw, Kne, Ksw, Kse, Kn2, Ks2, Knw2, Kne2,
    // Turn pieces (round 4): caps/rims leaning into a perpendicular run.
    CapE2, CapW2, RimE2, RimW2,
    None,
};

// The caller answers "is this cell open floor?" - out-of-bounds must answer
// false (the map border reads as solid rock). Kept as a plain function
// pointer + context so both the renderer lambda and the host tests fit
// without pulling <functional> into core/.
using OpenFn = bool (*)(const void* ctx, int32_t x, int32_t y);

struct Cell {
    uint8_t em = 0;        // effective mask (bleed applied - see effMask)
    Role role = Role::W0;  // pre-walk-behind role (computeWallCell's answer)
    bool isCap = false;
    bool isCorner = false;
};

// Floor adjacency of the four orthogonal neighbours.
uint8_t baseMask(OpenFn open, const void* ctx, int32_t x, int32_t y);

// baseMask plus the bleed rule: a wall sitting on another wall borrows E/W
// bits from the floor at the diagonal below, so side rims run two tiles
// tall just like north faces do.
uint8_t effMask(OpenFn open, const void* ctx, int32_t x, int32_t y);

// The pre-walk-behind role for a wall cell: mask tile, cap row, corner cap,
// side junction, or diagonal corner. (The lab's computeWallCell, verbatim.)
Cell computeWallCell(OpenFn open, const void* ctx, int32_t x, int32_t y);

// The walk-behind rewrite of a cell's role (the lab's wbRoleForCell).
Role wbRoleForCell(OpenFn open, const void* ctx, int32_t x, int32_t y, const Cell& cell);

// Both steps in one call - what the renderer actually draws at a wall cell.
inline Role wallRole(OpenFn open, const void* ctx, int32_t x, int32_t y) {
    return wbRoleForCell(open, ctx, x, y, computeWallCell(open, ctx, x, y));
}

// The overlay cast onto FLOOR cell (x,y) by the wall directly south of it,
// or Role::None. Crown/ridge strips (lone pillar, 1-tall runs) win; then the
// walk-behind rim of a peeled wall. Draw these AFTER the player with a
// south-of-player anchor - occluding the player is their entire point.
Role overlayRole(OpenFn open, const void* ctx, int32_t x, int32_t y);

// The lab's role string ("w3", "ccne2", "rimE2", ...) - test/debug only.
const char* roleName(Role r);

} // namespace core::wallauto
