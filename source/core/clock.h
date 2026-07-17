#pragma once

#include <cstdint>

namespace core {

// Wall-clock seconds since the Unix epoch - thin wrapper over time(NULL),
// confirmed to work on 3DS with zero special init beyond the default boot
// sequence (devkitPro's own examples/3ds/time/rtc example uses it
// directly, no RTC/SOC init call). Every core/ function that cares about
// "now" (cropStage, canHarvest, ...) takes it as a plain parameter instead
// of reaching for this itself - that's what makes them testable with an
// arbitrary "now" with no mocking needed. Only the 3DS-side call sites
// (world_scene.cpp) ever call this.
int64_t nowSeconds();

// Sleeping in a Bed fast-forwards the world: a persistent offset (saved
// in GameState::clockOffset, re-applied on load) added to the wall clock.
// Everything downstream - day/night, weather, crop growth, respawns -
// just sees a later "now".
void setClockOffset(int64_t offsetSec);
int64_t clockOffset();

} // namespace core
