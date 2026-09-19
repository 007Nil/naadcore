#!/usr/bin/env bash
# e2e_cli_coupler.sh — end-to-end harness for the CLI runtime coupler toggle.
#
# Self-contained: wipes ./build, configures, builds, and exercises the REAL
# CLI binary against the REAL harmonium plugin (FluidSynth "file" audio
# driver, so no audio hardware is needed).
#
# Checks:
#   1. status/coupler on/status/coupler off/status over stdin produces the
#      exact expected lines IN ORDER, and the process is killed by timeout
#      (exit 124) rather than exiting on its own.
#   2. Running with </dev/null stdin: the CLI prints the EOF message once
#      and stays alive (exit 124), instead of busy-spinning or exiting.
#   3. Sanity: the startup build-id banner matches `git rev-parse --short
#      HEAD` (catches stale-binary confusion at a glance).
#   4. Acoustic proof (tests/scripts/check_coupler_acoustic.sh): renders
#      the T16/T17 probes through the real plugin and asserts the
#      octave-up voice is AUDIBLE in the ON renders at PARITY with the
#      main voice (spectral presence + placement + octave-line growth +
#      per-note 48/60/72 octave-vs-main level within ±2 dB). Catches
#      wrong-pitch/silent/too-subtle coupler regressions that
#      state-level output checks cannot see.
#   5. FULL-STACK e2e: the entire chain the user relies on — stdin
#      "coupler on" -> PluginManager -> plugin set_config, AND real ALSA
#      NoteOn/NoteOff events from a virtual source client
#      (tests/scripts/midi_poke.cpp, built ad hoc into the scratch dir)
#      -> CLI MidiInput subscription -> PluginManager -> plugin layer
#      router -> FluidSynth "file" render -> acoustic parity assert on
#      the CLI-produced WAV. Exercises every layer in one pass.
#
# Usage: bash tests/e2e_cli_coupler.sh   (from anywhere; paths are resolved
# relative to the repo root, which is derived from this script's location)

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="$ROOT/build/apps/naadcore-cli/naadcore-cli"
PLUGIN="$ROOT/build/plugins/libharmonium_plugin.so"
SCRATCH="$(mktemp -d /tmp/opencode/e2e_cli_coupler.XXXXXX)"   # FluidSynth file driver writes fluidsynth.wav in CWD

FAILURES=0
fail() { printf 'FAILED: %s\n' "$*"; FAILURES=$((FAILURES + 1)); }
pass() { printf 'PASSED: %s\n' "$*"; }

# ---------------------------------------------------------------- build fresh
printf '==> rm -rf build (never test a stale binary)\n'
rm -rf "$ROOT/build"

printf '==> cmake configure\n'
cmake -S "$ROOT" -B "$ROOT/build" >/dev/null || { fail "cmake configure"; exit 1; }

printf '==> cmake --build\n'
cmake --build "$ROOT/build" -j"$(nproc)" >/dev/null || { fail "cmake build"; exit 1; }

[[ -x "$CLI" ]] || { fail "CLI binary not found: $CLI"; exit 1; }
[[ -f "$PLUGIN" ]] || { fail "plugin not found: $PLUGIN"; exit 1; }

# Assert the expected strings appear in the output file IN ORDER: find each
# one after the line where the previous one was found.
assert_in_order() {
    local file=$1; shift
    local expected=("$@")
    local prev=0 s ln
    for s in "${expected[@]}"; do
        ln=$(awk -v s="$s" -v start="$prev" 'NR > start && index($0, s) > 0 { print NR; exit }' "$file")
        if [[ -z "$ln" ]]; then
            fail "expected line not found (in order after line $prev): \"$s\""
            return 1
        fi
        prev=$ln
    done
    return 0
}

# ---------------------------------------------------------------- check 1
printf '==> Check 1: coupler on/off/status sequence via stdin\n'

printf 'status\ncoupler on\nstatus\ncoupler off\nstatus\n' \
    | timeout 10 "$CLI" --plugin "$PLUGIN" --audio-driver file \
      >"$SCRATCH/check1.out" 2>"$SCRATCH/check1.err"
rc=$?
if [[ $rc -ne 124 ]]; then
    fail "check 1: expected timeout kill (exit 124), got $rc"
else
    if assert_in_order "$SCRATCH/check1.out" \
        "Coupler: off" "Coupler ON" "Coupler: on" "Coupler OFF" "Coupler: off"
    then
        pass "check 1: coupler sequence correct, killed by timeout (124)"
    else
        fail "check 1: coupler output lines wrong/incomplete (see $SCRATCH/check1.out)"
    fi
fi

# ---------------------------------------------------------------- check 2
printf '==> Check 2: EOF on stdin stays alive (no spin, no exit)\n'

timeout 10 "$CLI" --plugin "$PLUGIN" --audio-driver file \
    </dev/null >"$SCRATCH/check2.out" 2>"$SCRATCH/check2.err"
rc=$?
eof_count=$(grep -c 'stdin closed (EOF)' "$SCRATCH/check2.out" || true)
if [[ $rc -ne 124 ]]; then
    fail "check 2: expected timeout kill (exit 124), got $rc"
elif [[ "$eof_count" != "1" ]]; then
    fail "check 2: EOF message printed $eof_count times (expected exactly 1)"
else
    pass "check 2: EOF message printed once, process stayed alive until timeout (124)"
fi

# ---------------------------------------------------------------- check 3
printf '==> Check 3: build id matches git HEAD\n'
if command -v git >/dev/null 2>&1 && git -C "$ROOT" rev-parse --short HEAD >/dev/null 2>&1; then
    expected_id="$(git -C "$ROOT" rev-parse --short HEAD)"
    banner_id="$(awk '/^naadcore-cli build / { print $3; exit }' "$SCRATCH/check1.out")"
    if [[ "$banner_id" == "$expected_id" ]]; then
        pass "check 3: build id $banner_id == git HEAD $expected_id"
    else
        fail "check 3: banner id '$banner_id' != git HEAD '$expected_id'"
    fi
else
    printf 'SKIPPED: check 3 (git unavailable or not a repo)\n'
fi

# ---------------------------------------------------------------- check 4
# Acoustic coupler proof: "coupler works" means the octave-up voice (note+12
# on ch 15) is AUDIBLE at PARITY in a render, not that the status string
# says "on". Catches the 2026-09-19 subsonic-tuning bug class (the coupler
# voice "played" at ~8 Hz) AND the 2026-09-20 too-subtle level class
# (CC7=60 put the octave ~9 dB under the main voice).
printf '==> Check 4: acoustic coupler proof (parity octave in render)\n'
if bash "$ROOT/tests/scripts/check_coupler_acoustic.sh"; then
    pass "check 4: coupler octave-up voice audible at parity in the ON renders"
else
    fail "check 4: coupler octave-up voice NOT audible/at parity (see check output above)"
fi

# ---------------------------------------------------------------- check 5
# Full-stack: stdin command -> PluginManager -> plugin set_config, AND
# real ALSA MIDI NoteOn/NoteOff from a virtual source client ->
# CLI MidiInput -> PluginManager -> plugin layer router -> audio render
# -> acoustic parity assert. Renders the OFF/ON pair through the whole
# chain and asserts the parity octave in the CLI-produced WAVs.
printf '==> Check 5: full-stack virtual-MIDI e2e (stdin + ALSA + render)\n'

if g++ -std=c++17 -Wall -Wextra -Wpedantic \
        "$ROOT/tests/scripts/midi_poke.cpp" -o "$SCRATCH/midi_poke" \
        -lasound 2>"$SCRATCH/poke_build.err"
then
    # One full-chain CLI run; leaves $SCRATCH/fs_<label>.wav behind.
    #   $1 = label (off|on)   $2 = CLI stdin command ("" = none)
    # Returns 0 only if the CLI logs prove the stdin command and the ALSA
    # link, and a finalized render exists.
    fullstack_render() {
        local label=$1 cli_cmd=$2
        local ready="$SCRATCH/ready_$label"
        local out="$SCRATCH/cli_$label.out"
        local err="$SCRATCH/cli_$label.err"
        rm -f "$ready" "$SCRATCH/fs_$label.wav" "$out" "$err" \
              "$SCRATCH/fluidsynth.wav"

        # MIDI source FIRST: the CLI subscribes to it at startup, so the
        # port must exist before --midi is parsed (its client:port is
        # announced on stdout). The timeline blocks on $ready — touched
        # only after the CLI prints "Listening for MIDI" — so no event is
        # lost to the subscription race (proven: a note sent before the
        # subscription renders as digital silence).
        ( printf 'waitfile %s 30\n' "$ready"
          printf 'on 60 100\n'
          printf 'wait 3000\n'
          printf 'off 60\n'
          printf 'wait 1500\n'
          printf 'exit\n' ) \
            | "$SCRATCH/midi_poke" >"$SCRATCH/poke_$label.out" \
                                     2>"$SCRATCH/poke_$label.err" &
        local poke_pid=$!

        # Read the announced source port (client:port), up to 5 s.
        local midi_src=""
        local i
        for i in $(seq 1 50); do
            midi_src=$(awk '/^PORT /{print $2; exit}' \
                "$SCRATCH/poke_$label.out" 2>/dev/null || true)
            [ -n "$midi_src" ] && break
            sleep 0.1
        done
        if [ -z "$midi_src" ]; then
            echo "check 5: midi_poke did not announce a port (see $SCRATCH/poke_$label.err)"
            kill -9 "$poke_pid" 2>/dev/null || true
            wait "$poke_pid" 2>/dev/null || true
            return 1
        fi

        # Start the real CLI from the scratch dir — the FluidSynth "file"
        # driver writes ./fluidsynth.wav in the process CWD. The CLI runs
        # in the background (NOT a subshell) so its PID is killable.
        local orig_cwd
        orig_cwd=$(pwd)
        cd "$SCRATCH"
        if [ -n "$cli_cmd" ]; then
            printf '%s\n' "$cli_cmd" | "$CLI" --plugin "$PLUGIN" \
                --audio-driver file --midi "$midi_src" >"cli_$label.out" \
                2>"cli_$label.err" &
        else
            "$CLI" --plugin "$PLUGIN" --audio-driver file \
                --midi "$midi_src" </dev/null >"cli_$label.out" \
                2>"cli_$label.err" &
        fi
        local cli_pid=$!
        cd "$orig_cwd"

        # Wait for the MIDI subscription to be announced (or early exit).
        local saw_listening=""
        for i in $(seq 1 100); do
            if grep -q 'Listening for MIDI' "$out" 2>/dev/null; then
                saw_listening=1
                break
            fi
            if ! kill -0 "$cli_pid" 2>/dev/null; then
                break
            fi
            sleep 0.1
        done
        if [ -z "$saw_listening" ]; then
            echo "check 5: CLI never announced MIDI readiness (see $err)"
            kill -9 "$cli_pid" 2>/dev/null || true
            wait "$cli_pid" 2>/dev/null || true
            kill -9 "$poke_pid" 2>/dev/null || true
            wait "$poke_pid" 2>/dev/null || true
            return 1
        fi
        touch "$ready"

        # Timeline: note on, 3 s hold, note off, 1.5 s tail, helper exits.
        wait "$poke_pid" || {
            echo "check 5: midi_poke timeline failed (see $SCRATCH/poke_$label.err)"
            kill -9 "$cli_pid" 2>/dev/null || true
            wait "$cli_pid" 2>/dev/null || true
            return 1
        }
        sleep 0.3   # let the final tail blocks land in the wav

        # Terminate the CLI: SIGTERM -> clean shutdown -> the "file"
        # driver finalizes fluidsynth.wav (fallback SIGKILL after 5 s).
        kill -TERM "$cli_pid" 2>/dev/null || true
        for i in $(seq 1 50); do
            kill -0 "$cli_pid" 2>/dev/null || break
            sleep 0.1
        done
        kill -9 "$cli_pid" 2>/dev/null || true
        wait "$cli_pid" 2>/dev/null || true

        if [ ! -f "$SCRATCH/fluidsynth.wav" ]; then
            echo "check 5: CLI produced no fluidsynth.wav (see $err)"
            return 1
        fi
        mv "$SCRATCH/fluidsynth.wav" "$SCRATCH/fs_$label.wav"

        # CLI logs must prove the stdin command and the ALSA link.
        if [ -n "$cli_cmd" ] && ! grep -q 'Coupler ON' "$out"; then
            echo "check 5: CLI never acknowledged '$cli_cmd' (see $out)"
            return 1
        fi
        if ! grep -q 'MIDI input connected' "$out"; then
            echo "check 5: CLI did not connect to the virtual MIDI source (see $out)"
            return 1
        fi
        if grep -q 'Note on failed' "$err"; then
            echo "check 5: plugin rejected the injected note (see $err)"
            return 1
        fi
        return 0
    }

    if fullstack_render off "" && fullstack_render on "coupler on" \
       && python3 "$ROOT/tests/scripts/coupler_acoustic_assert.py" \
              "$SCRATCH/fs_off.wav" "$SCRATCH/fs_on.wav"
    then
        pass "check 5: full-chain render carries the parity octave (stdin -> config -> ALSA MIDI -> plugin -> render)"
    else
        fail "check 5: full-stack virtual-MIDI e2e failed (details above)"
    fi
else
    fail "check 5: midi_poke build failed (ALSA dev headers missing?) — see $SCRATCH/poke_build.err"
fi

# ---------------------------------------------------------------- summary
printf '\n==================== e2e_cli_coupler summary ====================\n'
printf 'scratch output: %s\n' "$SCRATCH"
if (( FAILURES == 0 )); then
    printf 'ALL CHECKS PASSED\n'
    rm -rf "$SCRATCH"
    exit 0
else
    printf '%d CHECK(S) FAILED\n' "$FAILURES"
    exit 1
fi
