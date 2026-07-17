#include "wall_autotile.h"

// Port notes: this file mirrors tools/wall_lab/scenarios_template.html's
// <script> functions of the same names, line for line where C++ allows. If a
// change is wanted, make it THERE first, re-verify against the user's golden
// data (tests/js/), then mirror it here and regenerate tests/wall_cases.inc.

namespace core::wallauto {

namespace {

inline bool isWall(OpenFn open, const void* ctx, int32_t x, int32_t y) {
    return !open(ctx, x, y);
}

// A 1-tall wall run cell: floor both north and south of it.
inline bool is1TallRun(OpenFn open, const void* ctx, int32_t x, int32_t y) {
    return isWall(open, ctx, x, y) && (effMask(open, ctx, x, y) & 5) == 5;
}

inline Role wRole(uint8_t m) { return static_cast<Role>(m & 15); }

} // namespace

uint8_t baseMask(OpenFn open, const void* ctx, int32_t x, int32_t y) {
    return static_cast<uint8_t>((open(ctx, x, y - 1) ? 1 : 0) | (open(ctx, x + 1, y) ? 2 : 0) |
                                (open(ctx, x, y + 1) ? 4 : 0) | (open(ctx, x - 1, y) ? 8 : 0));
}

uint8_t effMask(OpenFn open, const void* ctx, int32_t x, int32_t y) {
    uint8_t m = baseMask(open, ctx, x, y);
    if (!(m & 4) && isWall(open, ctx, x, y + 1)) {
        if (!(m & 2) && open(ctx, x + 1, y + 1)) m |= 2;
        if (!(m & 8) && open(ctx, x - 1, y + 1)) m |= 8;
    }
    return m;
}

Cell computeWallCell(OpenFn open, const void* ctx, int32_t x, int32_t y) {
    Cell c;
    const uint8_t em = effMask(open, ctx, x, y);
    c.em = em;
    Role role = em == 14 ? Role::W15 : wRole(em);
    if (em == 5) role = Role::W4; // a 1-tall run middle shows a FACE
    // Side-wall junction only applies to a true vertical side wall (floor to
    // one side, NO floor to its own south) - otherwise a convex S+E / S+W
    // corner diagonally above a run end wrongly grabbed fcl/fcr.
    const bool eJ = !(em & 4) && (em & 2) && is1TallRun(open, ctx, x + 1, y + 1);
    const bool wJ = !(em & 4) && (em & 8) && is1TallRun(open, ctx, x - 1, y + 1);
    if (eJ && wJ) role = Role::FaceBoth; // 1-tall runs butt BOTH sides (cross)
    else if (eJ) role = Role::Fcl;
    else if (wJ) role = Role::Fcr;
    if (isWall(open, ctx, x, y + 1)) {
        const uint8_t bm = effMask(open, ctx, x, y + 1);
        const uint8_t add = bm & ~em & (2 | 8);
        if (bm & 4) {
            c.isCap = true;
            const uint8_t key = (em | add | 4) & 15;
            // The jamb piece (r2) belongs ONLY where the cap meets a
            // walk-behind run at its lower diagonal; every other S+E cap
            // takes the flat wide variant. r8j is the S+W mirror.
            if (key == 6) {
                role = is1TallRun(open, ctx, x + 1, y + 1) ? Role::R2 : Role::R2W;
            } else if (key == 12 && is1TallRun(open, ctx, x - 1, y + 1)) {
                role = Role::R8J;
            } else {
                switch (key) { // the lab's CAP_ROLE table
                    case 4: role = Role::R0; break;
                    case 5: role = Role::R1; break;
                    case 7: role = Role::R3; break;
                    case 12: role = Role::R8; break;
                    case 13: role = Role::R9; break;
                    case 14: role = Role::W14; break;
                    case 15: role = Role::W15; break;
                    default: role = wRole(key); break;
                }
            }
        } else if (add) {
            c.isCap = true;
            const bool variant = em != 0;
            // The cell below flanks a 2-tall wall -> the top-down corner
            // curve; both sides open -> symmetric cap. A plain corner-cap is
            // ALWAYS the curve, however thin the flanked wall (the round-1
            // "thin wall -> fcl/fcr" idea was reversed by round 4).
            if ((add & 2) && (add & 8)) role = Role::CapBoth;
            else if (add & 2) role = variant ? Role::Ccne2 : Role::Ccne;
            else role = variant ? Role::Ccnw2 : Role::Ccnw;
        }
    }
    if (!c.isCap) {
        const bool nwD = open(ctx, x - 1, y - 1) && !(em & 1) && !(em & 8) && !(em & 4);
        const bool neD = open(ctx, x + 1, y - 1) && !(em & 1) && !(em & 2) && !(em & 4);
        const bool swD = open(ctx, x - 1, y + 1) && !(em & 4) && !(em & 8);
        const bool seD = open(ctx, x + 1, y + 1) && !(em & 4) && !(em & 2);
        const bool cornerVariant = em != 0;
        if (nwD && neD) { role = Role::Kn2; c.isCorner = true; }
        else if (swD && seD) { role = Role::Ks2; c.isCorner = true; }
        else if (nwD) { role = cornerVariant ? Role::Knw2 : Role::Knw; c.isCorner = true; }
        else if (neD) { role = cornerVariant ? Role::Kne2 : Role::Kne; c.isCorner = true; }
        else if (swD) { role = cornerVariant ? Role::Ccnw2 : Role::Ksw; c.isCorner = true; }
        else if (seD) { role = cornerVariant ? Role::Ccne2 : Role::Kse; c.isCorner = true; }
    }
    c.role = role;
    return c;
}

Role wbRoleForCell(OpenFn open, const void* ctx, int32_t x, int32_t y, const Cell& cell) {
    if (cell.em & 4) return cell.role; // south-facing face: untouched
    if (cell.role == Role::Fcl || cell.role == Role::Fcr || cell.role == Role::FaceBoth)
        return cell.role;              // side junction wins
    const uint8_t em2 = cell.em & ~1;  // the N contact peels up as an overlay
    if (cell.isCap) {
        // Corner caps keep their identity; the variant only exists while some
        // E/W floor justifies it, so peeling N alone drops ccne2 -> ccne.
        if (cell.role == Role::Ccne || cell.role == Role::Ccne2)
            return em2 ? Role::Ccne2 : Role::Ccne;
        if (cell.role == Role::Ccnw || cell.role == Role::Ccnw2)
            return em2 ? Role::Ccnw2 : Role::Ccnw;
        if (cell.role == Role::CapBoth) return cell.role;
        if (em2 == 10) {
            // Floor on both sides: this cap sits at a turn. Lean toward
            // whichever side the wall mass continues diagonally below.
            const bool se = !open(ctx, x + 1, y + 1), sw = !open(ctx, x - 1, y + 1);
            if (se && !sw) return Role::CapE2;
            if (sw && !se) return Role::CapW2;
            return cell.role;
        }
        if (!(cell.em & 1)) return cell.role; // nothing peeled
        return em2 == 2 ? Role::R2W : em2 == 8 ? Role::R8 : Role::R0;
    }
    // A side floor whose run ENDS going down rounds into a corner; a leftover
    // bit on the opposite side picks the connected variant (kne2/knw2).
    const bool e = (cell.em & 2) && !open(ctx, x + 1, y + 1);
    const bool w = (cell.em & 8) && !open(ctx, x - 1, y + 1);
    if (e && w) return Role::Kn2;
    if (e) return (cell.em & 8) ? Role::Kne2 : Role::Kne;
    if (w) return (cell.em & 2) ? Role::Knw2 : Role::Knw;
    if (cell.em & (2 | 8)) return wRole(em2); // straight side edge, N peeled
    return Role::W0;                          // enclosed / diagonal-only: core
}

Role overlayRole(OpenFn open, const void* ctx, int32_t x, int32_t y) {
    const int32_t sy = y + 1;
    if (open(ctx, x, y) == false) return Role::None; // overlays live on floor
    if (!isWall(open, ctx, x, sy)) return Role::None;
    const uint8_t em = effMask(open, ctx, x, sy);
    // Crown / walk-behind ridge strips first (the lab's overlayAt).
    if (em == 15) return Role::R3;
    if (em == 5 || em == 7 || em == 13) {
        if ((em & 8) && !(em & 2)) return Role::RcapL;
        if ((em & 2) && !(em & 8)) return Role::RcapR;
        return Role::RcapM;
    }
    // Then the peeled rim (the lab's wbOverlayRole).
    if (!(em & 1) || (em & 4)) return Role::None;
    if ((em & 10) == 10) {
        // A rim with BOTH side bits where exactly one is bleed-borrowed is a
        // turn: the rim connects sideways into a perpendicular run one row
        // down, and the variant leans toward the borrowed (wall) side. A
        // single borrowed bit is just a 2-tall side rim - plain w9/w3.
        const uint8_t bm = baseMask(open, ctx, x, sy);
        const bool eB = !(bm & 2), wB = !(bm & 8);
        if (eB && !wB) return Role::RimE2;
        if (wB && !eB) return Role::RimW2;
    }
    return wRole(em);
}

const char* roleName(Role r) {
    switch (r) {
        case Role::W0: return "w0";   case Role::W1: return "w1";
        case Role::W2: return "w2";   case Role::W3: return "w3";
        case Role::W4: return "w4";   case Role::W5: return "w5";
        case Role::W6: return "w6";   case Role::W7: return "w7";
        case Role::W8: return "w8";   case Role::W9: return "w9";
        case Role::W10: return "w10"; case Role::W11: return "w11";
        case Role::W12: return "w12"; case Role::W13: return "w13";
        case Role::W14: return "w14"; case Role::W15: return "w15";
        case Role::R0: return "r0";   case Role::R1: return "r1";
        case Role::R2: return "r2";   case Role::R2W: return "r2w";
        case Role::R3: return "r3";   case Role::R8: return "r8";
        case Role::R8J: return "r8j"; case Role::R9: return "r9";
        case Role::RcapL: return "rcapL"; case Role::RcapM: return "rcapM";
        case Role::RcapR: return "rcapR";
        case Role::Fcl: return "fcl"; case Role::Fcr: return "fcr";
        case Role::FaceBoth: return "faceBoth";
        case Role::Ccne: return "ccne"; case Role::Ccnw: return "ccnw";
        case Role::Ccne2: return "ccne2"; case Role::Ccnw2: return "ccnw2";
        case Role::CapBoth: return "capBoth";
        case Role::Knw: return "knw"; case Role::Kne: return "kne";
        case Role::Ksw: return "ksw"; case Role::Kse: return "kse";
        case Role::Kn2: return "kn2"; case Role::Ks2: return "ks2";
        case Role::Knw2: return "knw2"; case Role::Kne2: return "kne2";
        case Role::CapE2: return "capE2"; case Role::CapW2: return "capW2";
        case Role::RimE2: return "rimE2"; case Role::RimW2: return "rimW2";
        case Role::None: return "";
    }
    return "";
}

} // namespace core::wallauto
