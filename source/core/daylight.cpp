#include "core/daylight.h"

namespace core {

float dayHour(int64_t now) {
    int64_t t = now % kDayLengthSec;
    if (t < 0) t += kDayLengthSec;
    return static_cast<float>(t) * 24.0f / static_cast<float>(kDayLengthSec);
}

float darkness(int64_t now) {
    float h = dayHour(now);
    // 6..20 day, 20..22 dusk ramp, 22..4 night, 4..6 dawn ramp.
    if (h >= 6.0f && h < 20.0f) return 0.0f;
    if (h >= 20.0f && h < 22.0f) return (h - 20.0f) / 2.0f;
    if (h >= 4.0f && h < 6.0f) return 1.0f - (h - 4.0f) / 2.0f;
    return 1.0f;
}

Weather weatherAt(uint32_t worldSeed, int64_t now) {
    // 15-minute fronts hashed with the same avalanche mix worldgen uses.
    uint32_t block = static_cast<uint32_t>(now / 900);
    uint32_t h = worldSeed ^ 0x9E3779B9u;
    h ^= block * 0x85EBCA6Bu;
    h ^= h >> 15;
    h *= 0xC2B2AE35u;
    h ^= h >> 13;
    return (h % 10) < 3 ? Weather::Rain : Weather::Clear;
}

} // namespace core
