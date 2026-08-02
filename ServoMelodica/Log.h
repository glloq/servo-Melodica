#ifndef CORE_LOG_H
#define CORE_LOG_H

#include <cstdint>

// Levelled logging that compiles on both the Arduino target (routes to Serial)
// and the host test build (routes to std::printf). The active level is chosen
// at compile time via LOG_LEVEL so DEBUG/INFO strings cost nothing in release
// binaries.
//
//   0 = OFF, 1 = ERROR, 2 = WARN, 3 = INFO, 4 = DEBUG
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#if defined(ARDUINO)
#include <Arduino.h>
#define MELODICA_LOG_PRINT(...) Serial.printf(__VA_ARGS__)
#else
#include <cstdio>
#define MELODICA_LOG_PRINT(...) std::printf(__VA_ARGS__)
#endif

#define LOG_ERROR(...)                          \
    do {                                        \
        if (LOG_LEVEL >= 1) {                   \
            MELODICA_LOG_PRINT("[ERROR] ");     \
            MELODICA_LOG_PRINT(__VA_ARGS__);    \
            MELODICA_LOG_PRINT("\n");           \
        }                                       \
    } while (0)

#define LOG_WARN(...)                           \
    do {                                        \
        if (LOG_LEVEL >= 2) {                   \
            MELODICA_LOG_PRINT("[WARN]  ");     \
            MELODICA_LOG_PRINT(__VA_ARGS__);    \
            MELODICA_LOG_PRINT("\n");           \
        }                                       \
    } while (0)

#define LOG_INFO(...)                           \
    do {                                        \
        if (LOG_LEVEL >= 3) {                   \
            MELODICA_LOG_PRINT("[INFO]  ");     \
            MELODICA_LOG_PRINT(__VA_ARGS__);    \
            MELODICA_LOG_PRINT("\n");           \
        }                                       \
    } while (0)

#define LOG_DEBUG(...)                          \
    do {                                        \
        if (LOG_LEVEL >= 4) {                   \
            MELODICA_LOG_PRINT("[DEBUG] ");     \
            MELODICA_LOG_PRINT(__VA_ARGS__);    \
            MELODICA_LOG_PRINT("\n");           \
        }                                       \
    } while (0)

#endif  // CORE_LOG_H
