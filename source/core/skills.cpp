#include "core/skills.h"

#include <cmath>

namespace core {

const char* const kSkillNames[kSkillCount] = {
    "Farming", "Logging", "Mining", "Foraging", "Herding", "Fishing", "Building",
};

namespace {
// Growth factor per level. 1.18 keeps early levels quick (2-3 gathers) and
// late levels genuinely long without RuneScape's full grind: level 10 needs
// ~180 XP, level 20 ~930, level 30 ~4900. u32 lifetime XP overflows the
// curve around level ~100, which is effectively "no real end" on a 3DS.
constexpr double kBase = 40.0;
constexpr double kGrowth = 1.18;
// Safety backstop only - the XP a u32 can hold runs out long before this.
constexpr int kMaxLevel = 200;
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
