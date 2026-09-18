#!/usr/bin/env bash
# Compile and run the harmonium plugin config-seam tests.
#
# Usage: run_config_tests.sh [path-to-libharmonium_plugin.so]
#
# The harness is compiled ad hoc (not wired into the project CMake build) so
# the build system stays untouched.

set -euo pipefail

REPO=$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)
PLUGIN=${1:-$REPO/build/plugins/libharmonium_plugin.so}
OUT=${OUT:-/tmp/opencode/test_plugin_config}

g++ -std=c++17 -Wall -Wextra -Wpedantic \
    -I "$REPO/include" \
    "$REPO/tests/test_plugin_config.cpp" \
    -o "$OUT" -ldl

"$OUT" "$PLUGIN"
