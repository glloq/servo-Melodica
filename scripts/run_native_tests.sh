#!/usr/bin/env bash
# Build and run the host-side unit tests for the instrument core.
# Requires only a C++17 compiler (no ESP32, no PlatformIO).
set -euo pipefail
cd "$(dirname "$0")/.."

CXX=${CXX:-g++}
OUT=$(mktemp -d)/native_tests

# The core lives flat in the ServoMelodica/ sketch folder. Compile the four
# platform-independent .cpp files (the .ino and Arduino-only headers are skipped).
"$CXX" -std=c++17 -Wall -Wextra -DLOG_LEVEL=0 \
    -IServoMelodica -Itest \
    test/*.cpp ServoMelodica/*.cpp \
    -o "$OUT"

"$OUT"
