#pragma once

#include <cstdint>

namespace core {

// RuneScape-style unbounded leveling, compressed: every action grants XP in
// its skill, each level needs ~18% more XP than the last (level 2 after a
// few actions, level 10 in a play session, level 30 a long-term goal - no
// hard cap). Levels gate node tiers (see balance.h), grow taming capacity,
// and add yield bonuses.
enum class Skill : uint8_t {
    Farming = 0,
    Logging = 1,
    Mining = 2,
    Foraging = 3,
    Herding = 4,
    Fishing = 5,
    Building = 6, // construction - placing buildings levels it, and higher
                  // levels unlock fancier things to build
    Athletics = 7, // sprinting overland (Circle Pad at its rim) - levels
                   // grow the stamina pool
    Swimming = 8,  // sprint-swimming - levels make hard swimming cheaper
};
constexpr int kSkillCount = 9;

extern const char* const kSkillNames[kSkillCount];

// XP needed to go from `level` to `level + 1` (level >= 1).
uint32_t xpToNext(int level);

// Current level (>= 1) for a lifetime XP total.
int levelForXp(uint32_t totalXp);

// For the UI bar: how far into the current level, and that level's size.
void xpProgress(uint32_t totalXp, int* intoLevel, int* levelSpan);

} // namespace core
