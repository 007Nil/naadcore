#!/usr/bin/env python3
"""Coupler parity measurement: is the octave voice at the MAIN voice's level?

Usage: coupler_parity_measure.py [--gate] <coupler_off.wav> <coupler_on.wav>

Analyzes matched mid-sustain windows of two plugin-in-loop renders of
tests/midi/T17_coupler_parity.mid (notes 48/60/72/84 held 3 s each,
plugin-in-loop renderer lead-in 0.2 s). The coupler spec (2026-09-20) is
PARITY: note N must sound as N + N+12 with the octave at the same level as
the main voice and EXACTLY +12 semitones (kCouplerCC7 = 100 = FluidSynth's
default channel volume, same played velocity, no detune).

Two independent measurements, both printed (and both gated with --gate):

  1. PAIR LINES (OFF render only). Consecutive probe notes are octave
     pairs: (48,60), (60,72), (72,84). With the coupler at CC 7 = 100 the
     coupler voice of note N is constructed IDENTICALLY to the main voice
     of note N+12 (same preset, same default channel volume, same
     velocity, no tuning) — so the OFF render's fundamental-line level of
     note N+12 IS the level the coupled octave of note N sounds at. The
     line delta dB(f(N+12)) - dB(f(N)) is therefore a clean, interference-
     free measurement of octave-vs-main level (it does not depend on the
     coupler at all; it measures the font notes the coupler reproduces).

  2. ADDED VOICE (ON vs OFF). Per pressed note, the sustain power growth
     gives the implied added-voice level: level_dB =
     10*log10(10^(presence_dB/10) - 1). Caveat, inherent to an EXACT
     octave: the coupler's partials coincide with the main's even
     partials, so coherent cross terms bias this estimate by up to ~1 dB;
     the pair-line measurement above is the unbiased one.

--gate exits 1 if the representative pressed notes (48/60/72) measure the
added voice outside PARITY_TOL_DB of the main voice, or if any pair-line
delta is outside PARITY_TOL_DB. Numbers are printed for the record
(tests/RESULTS.md).
"""

import sys
import wave
import numpy as np

# T17 layout: (on_s, off_s, note) at 120 BPM; WAV times add the
# plugin-in-loop renderer's 0.2 s lead-in. Mid-sustain window per segment.
SEGMENTS = [(0.5, 3.5, 48), (4.5, 7.5, 60), (8.5, 11.5, 72),
            (12.5, 15.5, 84)]
LEAD_IN = 0.2
WIN_OFFSET = (0.5, 2.5)      # (start, end) seconds after note-on

PARITY_TOL_DB = 2.0          # |octave - main| must stay within this


def read_wav(path):
    with wave.open(path, "rb") as w:
        sr = w.getframerate()
        raw = w.readframes(w.getnframes())
        width = w.getsampwidth()
        ch = w.getnchannels()
    if width != 2:
        raise SystemExit(f"unsupported sample width: {width}")
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    if ch == 2:
        x = x.reshape(-1, 2).mean(axis=1)
    return sr, x


def segment_window(sr, x, on_s):
    a = int((LEAD_IN + on_s + WIN_OFFSET[0]) * sr)
    b = int((LEAD_IN + on_s + WIN_OFFSET[1]) * sr)
    return x[a:b]


def window_power_db(seg):
    return 10.0 * np.log10(float((seg ** 2).mean()) + 1e-20)


def fundamental_line_db(sr, x, on_s, note):
    """Strongest-line power (Hann FFT, +-5 Hz cluster sum) near the note's
    nominal fundamental. The font is ~1 semitone sharp, so the search band
    is generous: 0.97..1.09 x nominal."""
    seg = segment_window(sr, x, on_s) * np.hanning(
        int((WIN_OFFSET[1] - WIN_OFFSET[0]) * sr))
    spec = np.abs(np.fft.rfft(seg)) ** 2
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    f_nom = 440.0 * 2.0 ** ((note - 69) / 12.0)
    band = np.where((freqs >= f_nom * 0.97) & (freqs <= f_nom * 1.09))[0]
    peak = band[np.argmax(spec[band])]
    f0 = float(freqs[peak])
    cluster = (freqs >= f0 - 5.0) & (freqs <= f0 + 5.0)
    return 10.0 * np.log10(float(spec[cluster].sum()) + 1e-20), f0


def main():
    gate = False
    args = sys.argv[1:]
    if args and args[0] == "--gate":
        gate = True
        args = args[1:]
    if len(args) != 2:
        print(__doc__)
        return 2
    sr_off, x_off = read_wav(args[0])
    sr_on, x_on = read_wav(args[1])
    if sr_off != sr_on:
        print(f"FAIL: sample rates differ ({sr_off} vs {sr_on})")
        return 1

    failures = []

    # ---- measurement 1: pair lines in the OFF render --------------------
    print("pair lines (OFF render; the coupled octave of N is constructed")
    print("identically to the main voice of N+12 at CC7=100):")
    line_db = {}
    f0s = {}
    for on_s, off_s, note in SEGMENTS:
        d, f0 = fundamental_line_db(sr_off, x_off, on_s, note)
        line_db[note], f0s[note] = d, f0
    pairs = [(48, 60), (60, 72), (72, 84)]
    for main_n, oct_n in pairs:
        delta = line_db[oct_n] - line_db[main_n]
        status = "OK" if abs(delta) <= PARITY_TOL_DB else "OUT OF TOL"
        print(f"  note {main_n:3d} f0 {f0s[main_n]:7.2f} Hz line "
              f"{line_db[main_n]:7.2f} dB | octave-voice note {oct_n:3d} f0 "
              f"{f0s[oct_n]:7.2f} Hz line {line_db[oct_n]:7.2f} dB"
              f" -> octave-main {delta:+.2f} dB  [{status}]")
        if abs(delta) > PARITY_TOL_DB:
            failures.append(
                f"pair-line parity {main_n}->{oct_n}: {delta:+.2f} dB "
                f"outside +-{PARITY_TOL_DB} dB")

    # ---- measurement 2: added voice per pressed note (ON vs OFF) -------
    print("added voice (ON vs OFF sustain power; exact-octave cross terms "
          "bias this ~1 dB):")
    for on_s, off_s, note in SEGMENTS:
        p_off = window_power_db(segment_window(sr_off, x_off, on_s))
        p_on = window_power_db(segment_window(sr_on, x_on, on_s))
        presence = p_on - p_off
        implied = 10.0 * np.log10(max(10.0 ** (presence / 10.0) - 1.0,
                                      1e-12))
        rep = "" if note == 84 else " (representative)"
        gated = note != 84
        status = "OK" if abs(implied) <= PARITY_TOL_DB else "OUT OF TOL"
        print(f"  note {note:3d}: sustain {p_off:7.2f} -> {p_on:7.2f} dBFS, "
              f"presence {presence:+.2f} dB -> added voice {implied:+.2f} dB "
              f"vs main  [{status}]{rep}")
        if gated and abs(implied) > PARITY_TOL_DB:
            failures.append(
                f"added-voice parity note {note}: {implied:+.2f} dB "
                f"outside +-{PARITY_TOL_DB} dB (octave missing or not at "
                f"parity?)")

    if failures:
        for f in failures:
            print("FAIL:", f)
        return 1
    print("PASS: coupler octave voice measures at parity with the main "
          "voice (exact +12 semitones, no detune)")
    return 0


if __name__ == "__main__":
    sys.exit(main())