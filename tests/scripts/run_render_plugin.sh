#!/usr/bin/env bash
# Compile and run the ad-hoc plugin-in-loop offline renderer.
#
# Usage: run_render_plugin.sh <track.mid> <output.wav> [tail_seconds]
#                             [KEY=VALUE ...]
#
# Renders a MIDI track through the REAL harmonium plugin (dlopen'd, FluidSynth
# "file" audio driver) so offline renders reflect all plugin behavior — voicing,
# envelope generators, uniform bellows velocity, and (Phase 3) any plugin
# config key passed as trailing KEY=VALUE pairs (applied via set_config before
# init; e.g. `run_render_plugin.sh T1.mid out.wav 3 stop=double`).
# Alternative to the live
# capture path (capture_live.sh) that needs no ALSA/PipeWire. Real-time render:
# a 20 s track takes ~23 s wall clock.
#
# The harness is compiled ad hoc (not wired into the project CMake build) so
# the build system stays untouched, mirroring run_config_tests.sh.

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: $0 <track.mid> <output.wav> [tail_seconds] [KEY=VALUE ...]" >&2
    exit 1
fi

REPO=$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)
PLUGIN=$REPO/build/plugins/libharmonium_plugin.so
BIN=${BIN:-/tmp/opencode/render_plugin}

if [ ! -f "$PLUGIN" ]; then
    echo "error: plugin missing — run: cmake -B build && cmake --build build -j4" >&2
    exit 1
fi

g++ -std=c++17 -Wall -Wextra -Wpedantic \
    -I "$REPO/include" \
    "$REPO/tests/scripts/render_plugin.cpp" \
    -o "$BIN" -ldl

"$BIN" "$PLUGIN" "$@"
