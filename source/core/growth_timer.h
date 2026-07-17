#pragma once

#include <cstdint>

namespace core {

// The one real-time formula the whole game runs on: crops growing, chopped
// trees regrowing, gathered nodes respawning, hens laying, cows filling
// udders. Extracted from crop.cpp (Milestone 1) once Milestone 2 grew three
// more users of the identical math. Clamped at both ends: a system clock
// that moved backwards never yields a negative elapsed, and a long absence
// never overshoots. Runs identically every frame during play and once on
// load to fast-forward - no separate "catch-up" code path anywhere.

inline int64_t elapsedSeconds(int64_t startedAt, int64_t now) {
    int64_t elapsed = now - startedAt;
    return elapsed < 0 ? 0 : elapsed;
}

inline int stageFromElapsed(int64_t startedAt, int64_t now, int32_t secondsPerStage, int numStages) {
    int64_t stage = elapsedSeconds(startedAt, now) / secondsPerStage;
    int64_t maxStage = numStages - 1;
    if (stage > maxStage) stage = maxStage;
    return static_cast<int>(stage);
}

inline bool elapsedAtLeast(int64_t startedAt, int64_t now, int32_t seconds) {
    return elapsedSeconds(startedAt, now) >= seconds;
}

} // namespace core
