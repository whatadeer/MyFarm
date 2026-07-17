#include "core/skills.h"

#include <cmath>

namespace core {

const char* const kSkillNames[kSkillCount] = {
    "Farming",  "Logging",  "Mining",    "Foraging", "Herding",
    "Fishing",  "Building", "Athletics", "Swimming", "Mycology",
};

namespace {
// Growth factor per level. 1.05 stretches the ladder into a real
// level-100 climb without losing the early hook: level 2 after a few
// gathers, ~62 XP per level around 10, ~440 around 50, ~5000 at the top -
// roughly 99k lifetime XP to cap. Content gates span the whole 1-100
// range now (balance.h), so the curve has to make 100 reachable.
constexpr double kBase = 40.0;
constexpr double kGrowth = 1.05;
// Level 100 is the cap - the ladder ends where the content does.
constexpr int kMaxLevel = 100;
} // namespace

uint32_t xpToNext(int level) {
    if (level < 1) level = 1;
    return static_cast<uint32_t>(kBase * std::pow(kGrowth, level - 1));
}

int levelForXp(uint32_t totalXp) {
    int level = 1;
    uint32_t remaining = totalXp;
    while (level < kMaxLevel) {
        uint32_t need = xpToNext(level);
        if (remaining < need) break;
        remaining -= need;
        level++;
    }
    return level;
}

void xpProgress(uint32_t totalXp, int* intoLevel, int* levelSpan) {
    int level = 1;
    uint32_t remaining = totalXp;
    while (level < kMaxLevel) {
        uint32_t need = xpToNext(level);
        if (remaining < need) break;
        remaining -= need;
        level++;
    }
    *intoLevel = static_cast<int>(remaining);
    *levelSpan = static_cast<int>(xpToNext(level));
}

} // namespace core
