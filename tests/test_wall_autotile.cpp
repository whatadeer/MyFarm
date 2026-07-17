// Pins core/wall_autotile.cpp to the lab rule (tools/wall_lab), which is the
// reference implementation the user verified: replay every scenario grid the
// user reviewed and require the identical role at every cell, wall layer and
// overlay layer both. wall_cases.inc is generated from the lab's own output
// by tools/wall_lab/gen_cpp_cases.js; the lab rule satisfies all 73 of the
// user's marked fixes and all 3005 implicitly-approved keeps, so matching it
// cell-for-cell means this port renders exactly what was accepted.
#include "core/wall_autotile.h"
#include "minitest.h"

#include <cstdint>
#include <cstring>

#include "wall_cases.inc"

namespace {

using core::wallauto::overlayRole;
using core::wallauto::Role;
using core::wallauto::roleName;
using core::wallauto::wallRole;

// A scenario grid: everything is wall except the listed floor cells.
// Out-of-bounds is wall (the map border reads as solid rock).
struct Grid {
    int w = 0, h = 0;
    bool open[16 * 16] = {};
};

bool gridOpen(const void* ctx, int32_t x, int32_t y) {
    const Grid* g = static_cast<const Grid*>(ctx);
    if (x < 0 || y < 0 || x >= g->w || y >= g->h) return false;
    return g->open[y * g->w + x];
}

int checkCase(const WallCase& c) {
    Grid g;
    g.w = c.w;
    g.h = c.h;
    for (int i = 0; i < c.nFloors; i++) {
        int fx = c.floors[i] >> 8, fy = c.floors[i] & 0xFF;
        g.open[fy * g.w + fx] = true;
    }
    int bad = 0;
    for (int y = 0; y < g.h; y++) {
        for (int x = 0; x < g.w; x++) {
            const char* wantW = c.wall[y * g.w + x];
            const char* gotW =
                g.open[y * g.w + x] ? "" : roleName(wallRole(gridOpen, &g, x, y));
            mt_checks++;
            if (std::strcmp(gotW, wantW) != 0) {
                mt_failures++;
                bad++;
                std::fprintf(stderr, "    FAIL %s (%d,%d) wall: lab says %s, port says %s\n",
                             c.id, x, y, wantW, gotW);
            }
            const char* wantO = c.overlay[y * g.w + x];
            const char* gotO = roleName(overlayRole(gridOpen, &g, x, y));
            mt_checks++;
            if (std::strcmp(gotO, wantO) != 0) {
                mt_failures++;
                bad++;
                std::fprintf(stderr, "    FAIL %s (%d,%d) overlay: lab says %s, port says %s\n",
                             c.id, x, y, wantO[0] ? wantO : "(none)", gotO[0] ? gotO : "(none)");
            }
        }
    }
    return bad;
}

} // namespace

int main() {
    std::printf("test_wall_autotile\n");
    int badCases = 0;
    for (int i = 0; i < kWallCaseCount; i++) {
        if (checkCase(kWallCases[i]) > 0) badCases++;
    }
    std::printf("  %d checks, %d failures across %d scenarios%s\n", mt_checks, mt_failures,
                kWallCaseCount, badCases ? "" : " - port matches the lab everywhere");
    return mt_failures == 0 ? 0 : 1;
}
