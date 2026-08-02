#ifndef NATIVE_TEST_H
#define NATIVE_TEST_H

// Tiny dependency-free test harness so the core can be built and run on a PC
// with a plain C++ compiler (no ESP32, no Unity required). PlatformIO's
// native_tests environment also compiles these files.
#include <cstdio>
#include <cstdlib>

namespace nt {
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
}  // namespace nt

#define CHECK(cond)                                                         \
    do {                                                                    \
        ++nt::checks();                                                     \
        if (!(cond)) {                                                      \
            ++nt::failures();                                               \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                       \
        ++nt::checks();                                                        \
        long long _va = (long long)(a);                                       \
        long long _vb = (long long)(b);                                       \
        if (_va != _vb) {                                                     \
            ++nt::failures();                                                 \
            std::printf("  FAIL %s:%d: %s (%lld) != %s (%lld)\n", __FILE__,   \
                        __LINE__, #a, _va, #b, _vb);                          \
        }                                                                     \
    } while (0)

#define RUN(fn)                             \
    do {                                    \
        std::printf("RUN  %s\n", #fn);      \
        fn();                               \
    } while (0)

#endif  // NATIVE_TEST_H
