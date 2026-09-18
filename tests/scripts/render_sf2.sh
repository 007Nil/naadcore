#!/usr/bin/env bash
# Offline render of a MIDI track through FluidSynth with the harmonium SoundFont.
#
# Usage: render_sf2.sh <track.mid> <output.wav> [baseline]
#
# Default mode pins the synth voicing to the harmonium plugin's Phase 1
# defaults (gain 0.4, reverb on with room 0.2 / damp 0.0 / width 0.3 /
# level 0.4, chorus off) so offline renders approximate the live plugin.
# Interpolation is 4th-order in both modes — the FluidSynth default, and the
# CLI exposes no per-channel interp setting.
#
# "baseline" mode omits all pinned voicing and uses stock FluidSynth defaults
# (gain 0.2, reverb room 0.5 / damp 0.3 / width 0.8 / level 0.7, chorus on),
# approximating the pre-Phase-1 plugin, which set no synth voicing at all.
#
# Output: 44100 Hz, s16, stereo WAV.

set -euo pipefail

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
    echo "usage: $0 <track.mid> <output.wav> [baseline]" >&2
    exit 1
fi

TRACK=$1
OUT=$2
MODE=${3:-phase1}
SF2=${HARMONIUM_SOUNDFONT:-/home/nil/harmonium-companion/harmonium.sf2}

if [ ! -f "$SF2" ]; then
    echo "error: SoundFont not found: $SF2" >&2
    exit 1
fi
if [ ! -f "$TRACK" ]; then
    echo "error: MIDI track not found: $TRACK" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT")"

if [ "$MODE" = "baseline" ]; then
    exec fluidsynth -ni -q -F "$OUT" -r 44100 -O s16 \
        "$SF2" "$TRACK"
fi

exec fluidsynth -ni -q -F "$OUT" -r 44100 -O s16 \
    -g 0.4 \
    -o synth.reverb.active=1 \
    -o synth.reverb.room-size=0.2 \
    -o synth.reverb.damp=0.0 \
    -o synth.reverb.width=0.3 \
    -o synth.reverb.level=0.4 \
    -o synth.chorus.active=0 \
    "$SF2" "$TRACK"
