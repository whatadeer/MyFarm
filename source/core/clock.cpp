#include "core/clock.h"

#include <ctime>

namespace core {

namespace {
int64_t g_offset = 0;
}

int64_t nowSeconds() {
    return static_cast<int64_t>(std::time(nullptr)) + g_offset;
}

void setClockOffset(int64_t offsetSec) { g_offset = offsetSec; }

int64_t clockOffset() { return g_offset; }

} // namespace core
