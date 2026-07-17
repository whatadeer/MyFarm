#pragma once

#include <cstdint>

namespace core {

// Day/night and weather, both pure functions of the wall clock (and the
// world seed, for weather) - nothing is stored, so they survive restarts
// for free, exactly like crop growth. The game runs on compressed time
// everywhere (wheat matures in 6 real minutes), so a full day/night cycle
// is 48 real minutes rather than tracking the console's 24h clock - every
// play session sees a sunrise or a sunset.
constexpr int32_t kDayLengthSec = 48 * 60;

// In-game hour in [0, 24): hour 0 lines up with epoch multiples of the day
// length. Daylight from 6:00 to 20:00, deep night from 22:00 to 4:00, with
// smooth two-hour dusk/dawn ramps between.
float dayHour(int64_t now);

// 0 = full daylight .. 1 = deep night (drives the top-screen darkening
// tint and the lamp/campfire glows).
float darkness(int64_t now);

// Weather changes on 15-minute fronts, deterministic from (seed, time):
// ~30% of fronts are rainy. In the Snowlands the same "rain" front falls
// as snow (a renderer choice - one weather system, two looks). Rain makes
// fish bite faster.
enum class Weather : uint8_t {
    Clear = 0,
    Rain = 1,
};
Weather weatherAt(uint32_t worldSeed, int64_t now);

} // namespace core
