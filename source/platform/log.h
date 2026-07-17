#pragma once

// Crash-surviving debug log - writes to sdmc:/3ds/myfarm/myfarm_log.txt,
// flushed after every line so a hard crash (ARM11 exception) still leaves a
// trail on the SD card showing how far execution got. Temporary
// instrumentation for tracking down the boot crash; thin enough to leave in
// or pull out later.
namespace platform {

void logInit();
void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logClose();

} // namespace platform

#define LOG(...) ::platform::logf(__VA_ARGS__)
