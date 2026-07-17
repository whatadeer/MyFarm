#include "platform/log.h"

#include <cstdarg>
#include <cstdio>

#include <sys/stat.h>

namespace platform {

namespace {
FILE* g_log = nullptr;
}

void logInit() {
    // fopen() never creates missing parent directories - save_io creates
    // these too, but logging starts before any save happens.
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/myfarm", 0777);
    g_log = fopen("sdmc:/3ds/myfarm/myfarm_log.txt", "w");
}

void logf(const char* fmt, ...) {
    if (!g_log) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);

    fputc('\n', g_log);
    fflush(g_log); // survive a hard crash - the last line shows how far we got
}

void logClose() {
    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }
}

} // namespace platform
