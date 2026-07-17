// Tiny assert-and-count harness for the host-native unit tests in this
// directory - no external test framework, since these are g++-compiled on
// whatever host runs `make -C tests` (see tests/Makefile) and pulling in a
// dependency just to call assert-and-continue would be more machinery than
// the job needs. Each test .cpp defines its own main() and RUNs a list of
// test_*() functions; mt_checks/mt_failures are file-local statics so
// multiple test binaries never collide.
#ifndef MINITEST_H
#define MINITEST_H

#include <cstdio>
#include <cstring>

static int mt_checks = 0;
static int mt_failures = 0;

#define CHECK(cond) do { \
        mt_checks++; \
        if (!(cond)) { \
            mt_failures++; \
            fprintf(stderr, "    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_STREQ(actual, expected) do { \
        mt_checks++; \
        const char *_a = (actual); \
        const char *_e = (expected); \
        if (strcmp(_a, _e) != 0) { \
            mt_failures++; \
            fprintf(stderr, "    FAIL %s:%d: expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, _e, _a); \
        } \
    } while (0)

#define RUN(fn) do { \
        printf("  %s\n", #fn); \
        fn(); \
    } while (0)

#endif
