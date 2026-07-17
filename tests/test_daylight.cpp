// Host-native tests for source/core/daylight.h: the day/night phase math
// and deterministic weather fronts.
#include "core/daylight.h"
#include "minitest.h"

using namespace core;

static void test_day_hour_wraps() {
    CHECK(dayHour(0) == 0.0f);
    // Half a day-length in = hour 12.
    CHECK(dayHour(kDayLengthSec / 2) == 12.0f);
    // Wraps every full day.
    CHECK(dayHour(kDayLengthSec * 3) == 0.0f);
}

static void test_darkness_phases() {
    auto atHour = [](float h) { return static_cast<int64_t>(h / 24.0f * kDayLengthSec); };
    CHECK(darkness(atHour(12.0f)) == 0.0f);  // noon
    CHECK(darkness(atHour(23.0f)) == 1.0f);  // deep night
    CHECK(darkness(atHour(1.0f)) == 1.0f);   // after midnight
    // Dusk ramps up between 20:00 and 22:00.
    float d21 = darkness(atHour(21.0f));
    CHECK(d21 > 0.4f && d21 < 0.6f);
    // Dawn ramps down between 4:00 and 6:00.
    float d5 = darkness(atHour(5.0f));
    CHECK(d5 > 0.4f && d5 < 0.6f);
    CHECK(darkness(atHour(7.0f)) == 0.0f);
}

static void test_weather_deterministic_and_mixed() {
    // Same inputs, same weather.
    CHECK(weatherAt(42, 100000) == weatherAt(42, 100000));
    // Stable within one 15-minute front.
    CHECK(weatherAt(42, 900 * 50) == weatherAt(42, 900 * 50 + 899));
    // Over many fronts both kinds occur, and rain stays the minority.
    int rain = 0;
    const int fronts = 200;
    for (int i = 0; i < fronts; i++) {
        if (weatherAt(7, static_cast<int64_t>(i) * 900) == Weather::Rain) rain++;
    }
    CHECK(rain > 20);
    CHECK(rain < 100);
}

int main() {
    printf("test_daylight:\n");
    RUN(test_day_hour_wraps);
    RUN(test_darkness_phases);
    RUN(test_weather_deterministic_and_mixed);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
