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
