#!/usr/bin/env bash
# Live capture of a MIDI track through the full CLI -> plugin -> audio chain.
#
# Usage: capture_live.sh <track.mid> [output.wav]
#
# Audio path discovery: if pactl is present (PipeWire/PulseAudio), the CLI is
# started with its default ALSA driver — PipeWire's ALSA plugin proxies the
# stream and it shows up in `pactl list short sink-inputs`, where it can be
# captured with `parecord --monitor-stream=<index>`. If no such stream
# appears (pure ALSA machine), a snd-aloop + ~/.asoundrc redirect fallback
# is attempted and cleaned up afterwards.
#
# MIDI: aplaymidi sends the track into the 'naadcore input' port the CLI
# registers; the CLI's --midi argument subscribes to the Midi Through port
# so no hardware keyboard is needed.
#
# NOTE: the CLI's --audio-driver flag is parsed but NOT wired through to the
# plugin (known dead-flag bug) — the plugin always uses its internal default
# ALSA driver, which is what this script relies on.
#
# Output default: tests/renders/<yyyymmdd>_<track>.wav

set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 <track.mid> [output.wav]" >&2
    exit 1
fi

TRACK=$1
REPO=$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)
CLI=$REPO/build/apps/naadcore-cli/naadcore-cli
PLUGIN=$REPO/build/plugins/libharmonium_plugin.so
RENDERS=$REPO/tests/renders
OUT=${2:-$RENDERS/$(date +%Y%m%d)_$(basename "${TRACK%.mid}").wav}
LOG=${LOG:-/tmp/opencode/naadcore-capture.log}

CLI_PID=""
REC_PID=""
ASOUNDRC_RESTORED=0

cleanup() {
    if [ -n "$REC_PID" ]; then
        kill -INT "$REC_PID" 2>/dev/null || true
    fi
    if [ -n "$CLI_PID" ]; then
        kill "$CLI_PID" 2>/dev/null || true
    fi
    pkill -x naadcore-cli 2>/dev/null || true
    wait 2>/dev/null || true
    if [ "$ASOUNDRC_RESTORED" -eq 1 ]; then
        mv "$HOME/.asoundrc.naadcore.bak" "$HOME/.asoundrc" 2>/dev/null || true
    elif [ -f "$HOME/.asoundrc.naadcore.new" ]; then
        rm -f "$HOME/.asoundrc.naadcore.new" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

if [ ! -f "$TRACK" ]; then
    echo "error: track not found: $TRACK" >&2
    exit 1
fi
if [ ! -x "$CLI" ] || [ ! -f "$PLUGIN" ]; then
    echo "error: build missing — run: cmake -B build && cmake --build build -j4" >&2
    exit 1
fi
mkdir -p "$(dirname "$OUT")"

# First port of a named ALSA client, e.g. port_for 'Midi Through' -> 14:0
port_for() {
    aconnect -l | awk -v want="$1" '
        /^client / {
            match($0, /client [0-9]+/); num = substr($0, RSTART + 7, RLENGTH - 7)
            mine = (index($0, want) > 0)
        }
        mine && /^ +[0-9]+ +\x27/ {
            port = $1
            gsub(/[^0-9]/, "", port)
            print num ":" port
            exit
        }'
}

MIDI_SRC=$(port_for "Midi Through")
if [ -z "$MIDI_SRC" ]; then
    echo "error: no Midi Through port found (aconnect -l)" >&2
    exit 1
fi

start_cli() {
    "$CLI" --plugin "$PLUGIN" --midi "$MIDI_SRC" > "$LOG" 2>&1 &
    CLI_PID=$!
    sleep 2
    if ! kill -0 "$CLI_PID" 2>/dev/null; then
        echo "error: naadcore-cli died at startup — see $LOG" >&2
        exit 1
    fi
}

play_track_and_tail() {
    local port
    port=$(port_for "naadcore")
    if [ -z "$port" ]; then
        echo "error: naadcore port not found (aconnect -l) — see $LOG" >&2
        exit 1
    fi
    T_PLAY=$(date +%s.%N)
    aplaymidi -p "$port" "$TRACK"
    sleep "${TAIL_SECONDS:-3}"
}

# Wall-clock offset between recorder start and playback start, so the WAV
# timeline can be aligned with the MIDI track timing during analysis
# (analyze.py auto-reads the "<wav>.offset" sidecar this writes).
write_offset() {
    python3 - "$T_REC" "$T_PLAY" "$OUT.offset" <<'PY'
import sys
rec, play, path = sys.argv[1:4]
with open(path, "w") as f:
    f.write(f"{float(play) - float(rec):.3f}\n")
PY
}

echo "capture: $TRACK -> $OUT"

if command -v pactl >/dev/null 2>&1; then
    BEFORE=$(pactl list short sink-inputs | awk '{print $1}' | sort)
fi

start_cli

if command -v pactl >/dev/null 2>&1; then
    # give PipeWire a moment to proxy the CLI's ALSA stream
    sleep 1
    AFTER=$(pactl list short sink-inputs | awk '{print $1}' | sort)
    NEW=$(comm -13 <(echo "$BEFORE") <(echo "$AFTER") | tail -1)
    if [ -n "$NEW" ]; then
        echo "pipewire/pulse path: capturing sink-input $NEW with parecord"
        T_REC=$(date +%s.%N)
        parecord --monitor-stream="$NEW" --file-format=wav "$OUT" &
        REC_PID=$!
        sleep 1
        play_track_and_tail
        write_offset
        kill -INT "$REC_PID" 2>/dev/null || true
        wait "$REC_PID" 2>/dev/null || true
        REC_PID=""
        kill "$CLI_PID" 2>/dev/null || true
        wait "$CLI_PID" 2>/dev/null || true
        CLI_PID=""
        echo "done: $OUT"
        exit 0
    fi
    echo "no sink-input appeared via pactl; trying snd-aloop fallback"
fi

# Pure-ALSA fallback: redirect the default PCM to a snd-aloop loopback and
# record from the loopback's capture side.
if ! grep -q Loopback /proc/asound/cards 2>/dev/null; then
    modprobe snd-aloop 2>/dev/null || true
fi
if ! grep -q Loopback /proc/asound/cards 2>/dev/null; then
    echo "error: snd-aloop unavailable (modprobe needs root) and no pactl" >&2
    echo "stream appeared — cannot capture on this machine. See" >&2
    echo "tests/README.md for the manual arecord/loopback procedure." >&2
    exit 1
fi

if [ -f "$HOME/.asoundrc" ]; then
    cp "$HOME/.asoundrc" "$HOME/.asoundrc.naadcore.bak"
    ASOUNDRC_RESTORED=1
fi
cat > "$HOME/.asoundrc.naadcore.new" <<'EOF'
pcm.!default {
    type plug
    slave.pcm "hw:Loopback,0,0"
}
EOF
mv "$HOME/.asoundrc.naadcore.new" "$HOME/.asoundrc"

# restart the CLI so FluidSynth's ALSA driver picks up the redirect
kill "$CLI_PID" 2>/dev/null || true
wait "$CLI_PID" 2>/dev/null || true
CLI_PID=""
start_cli

T_REC=$(date +%s.%N)
arecord -D plughw:Loopback,1,0 -f S16_LE -r 44100 -c 2 "$OUT" &
REC_PID=$!
sleep 1
play_track_and_tail
write_offset
kill -INT "$REC_PID" 2>/dev/null || true
wait "$REC_PID" 2>/dev/null || true
REC_PID=""
kill "$CLI_PID" 2>/dev/null || true
wait "$CLI_PID" 2>/dev/null || true
CLI_PID=""
echo "done: $OUT"
