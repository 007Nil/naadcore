#!/usr/bin/env bash
# check_coupler_acoustic.sh — render-based acoustic proof that the coupler
# adds an AUDIBLE octave-up voice (note+12 on ch 15) when ON.
#
# "Coupler works" must mean "the octave is audible in the render", not
# "the status string says on". This check renders the same probe twice
# through the REAL plugin (plugin-in-loop FluidSynth "file" driver —
# no hardware) — once with the default coupler=off, once with coupler=on
# applied via the pre-init set_config path (the same state the CLI's
# "coupler on" command gives to NEW presses) — and asserts spectrally that
# the ON render contains the octave voice (details + thresholds in
# tests/scripts/coupler_acoustic_assert.py). It was created after the
# 2026-09-19 diagnosis: a misused MIDI Tuning Standard call tuned every
# coupler voice to ~8 Hz — a subsonic rumble that no status-string test
# could catch.
#
# Usage: bash tests/scripts/check_coupler_acoustic.sh   (from anywhere)
# Requires: built plugin (build/plugins/libharmonium_plugin.so),
# python3 + numpy, g++; takes ~15-20 s (two real-time renders).
#
# Wired into tests/e2e_cli_coupler.sh as Check 4 (after its fresh rebuild).

set -euo pipefail

ROOT="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../.." && pwd)"
PLUGIN="$ROOT/build/plugins/libharmonium_plugin.so"
PROBE="$ROOT/tests/midi/T16_coupler_acoustic.mid"

if [ ! -f "$PLUGIN" ]; then
    echo "error: plugin missing — run: cmake -B build && cmake --build build -j4" >&2
    exit 1
fi
if [ ! -f "$PROBE" ]; then
    echo "error: probe track missing: $PROBE" >&2
    echo "       regenerate with: python3 tests/scripts/gen_probe_coupler_acoustic.py" >&2
    exit 1
fi

SCRATCH="$(mktemp -d /tmp/opencode/coupler_acoustic.XXXXXX)"
trap 'rm -rf "$SCRATCH"' EXIT

printf '==> render coupler=off baseline\n'
"$ROOT/tests/scripts/run_render_plugin.sh" "$PROBE" "$SCRATCH/off.wav" 1 \
    >/dev/null 2>&1

printf '==> render coupler=on\n'
"$ROOT/tests/scripts/run_render_plugin.sh" "$PROBE" "$SCRATCH/on.wav" 1 \
    coupler=on >/dev/null 2>&1

printf '==> spectral assertion\n'
python3 "$ROOT/tests/scripts/coupler_acoustic_assert.py" \
    "$SCRATCH/off.wav" "$SCRATCH/on.wav"