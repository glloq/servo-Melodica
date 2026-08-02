#!/usr/bin/env bash
# Build and run the host-side unit tests for the instrument core.
# Requires only a C++17 compiler (no ESP32, no PlatformIO).
set -euo pipefail
cd "$(dirname "$0")/.."

CXX=${CXX:-g++}
OUT=$(mktemp -d)/native_tests

"$CXX" -std=c++17 -Wall -Wextra -DLOG_LEVEL=0 \
    -Ilib/core -Ilib/drivers_air -Itest \
    test/*.cpp lib/core/*.cpp \
    -o "$OUT"

"$OUT"
