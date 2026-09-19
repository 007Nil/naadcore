#!/usr/bin/env bash
# check_coupler_acoustic.sh — render-based acoustic proof of the coupler.
#
# "Coupler works" must mean "the octave is audible in the render", not
# "the status string says on". Since 2026-09-20 the coupler spec is
# PARITY: the octave voice sounds at the SAME level as the main voice,
# exactly +12 semitones (no detune). This check renders the probes twice
# through the REAL plugin (plugin-in-loop FluidSynth "file" driver — no
# hardware) — once with the default coupler=off, once with coupler=on
# applied via the pre-init set_config path (the same state the CLI's
# "coupler on" command gives to NEW presses) — and asserts:
#
#   1. AUDIBILITY (T16, single note 60, tests/scripts/
#      coupler_acoustic_assert.py): the ON render contains a PARITY-level
#      octave voice — sustain power grows +1.5..+4.5 dB (two equal-power
#      voices = +3 dB; the old CC7=60 build measured +0.5 dB and must
#      fail), the added power lives at the octave band (>=80% above
#      1.4*f0 — catches the 2026-09-19 subsonic-tuning bug class), and
#      the coupler's 2nd-partial cluster (≈4*f0) grows (catches
#      silent/wrong-pitch regressions).
#   2. PARITY (T17, notes 48/60/72/84, tests/scripts/
#      coupler_parity_measure.py --gate): per representative note
#      (48/60/72) the ADDED voice implied by the ON/OFF sustain-power
#      growth must sit within ±2 dB of the main voice, and the OFF
#      render's octave-pair fundamental lines (48→60, 60→72, 72→84 —
#      the coupled octave of N is constructed identically to the main
#      voice of N+12 at CC7=100) must agree within ±2 dB.
#
# Usage: bash tests/scripts/check_coupler_acoustic.sh   (from anywhere)
# Requires: built plugin (build/plugins/libharmonium_plugin.so),
# python3 + numpy, g++; takes ~60 s (four real-time renders).
#
# Wired into tests/e2e_cli_coupler.sh as Check 4 (after its fresh rebuild).

set -euo pipefail

ROOT="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../.." && pwd)"
PLUGIN="$ROOT/build/plugins/libharmonium_plugin.so"
PROBE="$ROOT/tests/midi/T16_coupler_acoustic.mid"
PROBE_PARITY="$ROOT/tests/midi/T17_coupler_parity.mid"

for f in "$PLUGIN" "$PROBE" "$PROBE_PARITY"; do
    if [ ! -f "$f" ]; then
        echo "error: missing $f" >&2
        echo "       (build with cmake -B build && cmake --build build; regenerate" >&2
        echo "        probes with gen_probe_coupler_acoustic.py /" >&2
        echo "        gen_probe_coupler_parity.py)" >&2
        exit 1
    fi
done

SCRATCH="$(mktemp -d /tmp/opencode/coupler_acoustic.XXXXXX)"
trap 'rm -rf "$SCRATCH"' EXIT

printf '==> render T16 coupler=off baseline\n'
"$ROOT/tests/scripts/run_render_plugin.sh" "$PROBE" "$SCRATCH/off.wav" 1 \
    >/dev/null 2>&1

printf '==> render T16 coupler=on\n'
"$ROOT/tests/scripts/run_render_plugin.sh" "$PROBE" "$SCRATCH/on.wav" 1 \
    coupler=on >/dev/null 2>&1

printf '==> T16 spectral assertion (audibility)\n'
python3 "$ROOT/tests/scripts/coupler_acoustic_assert.py" \
    "$SCRATCH/off.wav" "$SCRATCH/on.wav"

printf '==> render T17 coupler=off baseline\n'
"$ROOT/tests/scripts/run_render_plugin.sh" "$PROBE_PARITY" \
    "$SCRATCH/parity_off.wav" 1 >/dev/null 2>&1

printf '==> render T17 coupler=on\n'
"$ROOT/tests/scripts/run_render_plugin.sh" "$PROBE_PARITY" \
    "$SCRATCH/parity_on.wav" 1 coupler=on >/dev/null 2>&1

printf '==> T17 parity assertion (48/60/72 octave at main level)\n'
python3 "$ROOT/tests/scripts/coupler_parity_measure.py" --gate \
    "$SCRATCH/parity_off.wav" "$SCRATCH/parity_on.wav"