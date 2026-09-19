#!/usr/bin/env python3
"""Coupler acoustic assertion: is the octave-up voice at PARITY in the render?

Usage: coupler_acoustic_assert.py <coupler_off.wav> <coupler_on.wav>
                               [win_start win_end]

Compares matched mid-sustain windows of two renders of the coupler probe
(tests/midi/T16_coupler_acoustic.mid, single note 60): the OFF baseline and
the ON render (coupler=on — pre-init for the plugin-in-loop renderer, the
live "coupler on" CLI command for the full-stack e2e). Since 2026-09-20 the
coupler spec is PARITY: the octave voice (note+12 on FluidSynth channel 15)
sounds at the SAME level as the main voice (CC 7 = 100 = FluidSynth's
default channel volume, the note's own played velocity) and at EXACTLY +12
semitones (no detune) — so its partials coincide with the main's even
partials instead of forming separate +3-cent sideband lines.

Window: by default the note onset (first energy above the noise floor) is
auto-detected in EACH file and the window is onset+0.5 .. onset+2.5 s
(robust to startup-time skew between renders — the full-stack e2e renders
start whenever the CLI's audio driver comes up). Optional explicit
[win_start win_end] arguments override with absolute WAV times.

Assertions (all must hold; exits 0 on PASS, 1 on FAIL):
  1. PRESENCE/PARITY — total sustain power grows by MIN_PRESENCE_DB ..
      MAX_PRESENCE_DB. Two equal-power voices give +3.0 dB; the band
      +1.5..+4.5 dB accepts font-level and interference spread while
      failing the old too-subtle layer (+0.5 dB at CC7=60), a missing
      octave (~0 dB) and the historical subsonic rumble (+0.57 dB).
  2. PLACEMENT — of the per-bin clipped added spectral power, at least
      (1 - MAX_LOW_FRACTION) must live above 1.4*f0, where the octave
      voice lives. The 2026-09-19 subsonic-tuning bug (a misused
      fluid_synth_activate_key_tuning "tuned" every coupler voice to ~8 Hz)
      put essentially ALL added power below 260 Hz: audible-spectrum
      placement is what catches that failure class. Per-bin clipping is
      essential: with the exact +12-semitone coupler its partials coincide
      with the main's even partials, so raw band-power differences suffer
      coherent cancellation (a band can SHRINK although the voice was
      added).
  3. OCTAVE LINES — in the coupler's 2nd-partial zone (just above 4*f0,
      where the main voice's own 4th partial is weak), at least MIN_GROW_BINS
      FFT bins must grow by >= MIN_LINE_GROWTH_DB over the OFF render: the
      parity octave voice's 2nd partial dwarfs the main's 4th there.

Numbers are printed for the record (tests/RESULTS.md).
"""

import sys
import wave
import numpy as np

# Analysis window offsets (seconds, relative to the auto-detected note
# onset; the plugin-in-loop renderer has a 0.2 s lead-in, note-on at 0.7 s
# WAV time, note-off at 3.7 s, release 200 ms — full-stack e2e renders have
# no fixed lead-in, hence onset-relative windows).
WIN_OFFSETS = (0.5, 2.5)

# Onset detection: first 10 ms RMS block above this dBFS threshold.
ONSET_THRESH_DBFS = -70.0
ONSET_BLOCK_MS = 10

# Thresholds (measured on the parity build 2026-09-20, T16 note 60:
# presence +2.80 dB, low fraction 0.0001, 31 bins >= +6 dB in the 4*f0 zone
# (max +57 dB); the old CC7=60 build measured presence +0.50 dB, the
# subsonic-tuning bug build +0.57 dB presence with ~100% of added power
# below 260 Hz and zero line growth).
MIN_PRESENCE_DB = 1.5
MAX_PRESENCE_DB = 4.5
MAX_LOW_FRACTION = 0.2
MIN_LINE_GROWTH_DB = 6.0
MIN_GROW_BINS = 3

F0_RANGE = (150.0, 420.0)  # main fundamental search band for note 60 (f0 ~ 277 Hz)
LOW_SPLIT_FACTOR = 1.4      # low band = 20 Hz .. 1.4*f0 (below the coupler fundamental 2*f0)


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


def detect_onset(x, sr):
    """Index of the first ONSET_BLOCK_MS block whose RMS amplitude exceeds
    the threshold (renders start with digital silence before the note)."""
    block = max(1, int(sr * ONSET_BLOCK_MS / 1000.0))
    n = len(x) - len(x) % block
    if n == 0:
        raise SystemExit("empty render")
    rms = np.sqrt((x[:n].reshape(-1, block) ** 2).mean(axis=1))
    thresh = 10.0 ** (ONSET_THRESH_DBFS / 20.0)   # amplitude, matching rms
    above = np.nonzero(rms > thresh)[0]
    if len(above) == 0:
        raise SystemExit("no note onset found (silent render?)")
    return int(above[0]) * block


def window_power(x, sr, onset):
    a = onset + int(WIN_OFFSETS[0] * sr)
    b = onset + int(WIN_OFFSETS[1] * sr)
    seg = x[a:b]
    return float((seg ** 2).mean())


def spectrum(x, sr, onset):
    a = onset + int(WIN_OFFSETS[0] * sr)
    b = onset + int(WIN_OFFSETS[1] * sr)
    seg = x[a:b] * np.hanning(b - a)
    spec = np.abs(np.fft.rfft(seg)) ** 2
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    return freqs, spec


def main():
    if len(sys.argv) not in (3, 5):
        print(__doc__)
        return 2
    sr_off, x_off = read_wav(sys.argv[1])
    sr_on, x_on = read_wav(sys.argv[2])
    if sr_off != sr_on:
        print(f"FAIL: sample rates differ ({sr_off} vs {sr_on})")
        return 1

    if len(sys.argv) == 5:
        onset_off = int(float(sys.argv[3]) * sr_off)
        onset_on = int(float(sys.argv[3]) * sr_on)
        # explicit absolute window: shift onsets so WIN_OFFSETS lands there
        onset_off -= int(WIN_OFFSETS[0] * sr_off)
        onset_on -= int(WIN_OFFSETS[0] * sr_on)
    else:
        onset_off = detect_onset(x_off, sr_off)
        onset_on = detect_onset(x_on, sr_on)
    skew_ms = abs((onset_on - onset_off) / sr_on) * 1000.0
    print(f"onsets: off {onset_off/sr_off:.2f} s, on {onset_on/sr_on:.2f} s "
          f"(skew {skew_ms:.0f} ms); windows "
          f"[{WIN_OFFSETS[0]}, {WIN_OFFSETS[1]}] s after onset")

    p_off = window_power(x_off, sr_off, onset_off)
    p_on = window_power(x_on, sr_on, onset_on)
    presence_db = 10.0 * np.log10(p_on / p_off)

    freqs, s_off = spectrum(x_off, sr_off, onset_off)
    _, s_on = spectrum(x_on, sr_on, onset_on)

    # main fundamental: strongest line in F0_RANGE of the OFF render
    band = np.where((freqs >= F0_RANGE[0]) & (freqs <= F0_RANGE[1]))[0]
    f0 = float(freqs[band[np.argmax(s_off[band])]])

    # per-bin clipped added power, split low (below 1.4*f0) vs high
    added = np.clip(s_on - s_off, 0.0, None)
    low_mask = (freqs >= 20.0) & (freqs < LOW_SPLIT_FACTOR * f0)
    high_mask = (freqs >= LOW_SPLIT_FACTOR * f0) & (freqs <= 20000.0)
    added_low = float(added[low_mask].sum())
    added_high = float(added[high_mask].sum())
    low_fraction = added_low / max(added_low + added_high, 1e-12)

    # octave-line growth in the coupler's 2nd-partial zone: the coupler
    # fundamental (2*f0) hides inside the main's 2nd-partial beating
    # cluster, but its 2nd partial (~4*f0) lands on the main's weak 4th
    # partial — at parity it dwarfs it and the cluster grows strongly.
    zone = (freqs >= 3.95 * f0) & (freqs <= 4.10 * f0)
    growth_db = 10.0 * np.log10((s_on[zone] + 1e-20) / (s_off[zone] + 1e-20))
    grow_bins = int((growth_db >= MIN_LINE_GROWTH_DB).sum())
    max_growth = float(growth_db.max()) if zone.any() else -np.inf

    print(f"main f0: {f0:.2f} Hz")
    print(f"sustain power off/on: {10*np.log10(p_off):.2f} / "
          f"{10*np.log10(p_on):.2f} dBFS -> presence {presence_db:+.2f} dB")
    print(f"added spectral power: low(<{LOW_SPLIT_FACTOR*f0:.0f} Hz) "
          f"{added_low:.3g}, high {added_high:.3g} -> low fraction "
          f"{low_fraction:.4f}")
    print(f"coupler 2nd-partial zone {3.95*f0:.0f}-{4.10*f0:.0f} Hz: "
          f"{grow_bins} bins grew >= {MIN_LINE_GROWTH_DB} dB "
          f"(max {max_growth:+.1f} dB)")

    failures = []
    if presence_db < MIN_PRESENCE_DB:
        failures.append(
            f"presence {presence_db:+.2f} dB < {MIN_PRESENCE_DB} dB — "
            f"no parity-level octave voice was added (coupler noteon path "
            f"broken, gain regressed below CC7=100, or subsonic rumble?)")
    if presence_db > MAX_PRESENCE_DB:
        failures.append(
            f"presence {presence_db:+.2f} dB > {MAX_PRESENCE_DB} dB — "
            f"the layer is far too loud (layer gain / routing wrong?)")
    if low_fraction > MAX_LOW_FRACTION:
        failures.append(
            f"added power is {100*low_fraction:.1f}% below "
            f"{LOW_SPLIT_FACTOR*f0:.0f} Hz — the added voice is NOT the "
            f"octave (subsonic/wrong-pitch tuning bug class; see the "
            f"2026-09-19 kCouplerDetuneCents incident in tests/RESULTS.md)")
    if grow_bins < MIN_GROW_BINS:
        failures.append(
            f"only {grow_bins} bins grew >= {MIN_LINE_GROWTH_DB} dB in the "
            f"coupler 2nd-partial zone (need {MIN_GROW_BINS}) — no octave-up "
            f"line cluster appeared")

    if failures:
        for f in failures:
            print("FAIL:", f)
        return 1
    print("PASS: coupler octave-up voice is present at parity in the ON "
          f"render (presence {presence_db:+.2f} dB, {100*(1-low_fraction):.2f}% "
          f"of added power above {LOW_SPLIT_FACTOR*f0:.0f} Hz, {grow_bins} "
          f"octave-zone bins grew)")
    return 0


if __name__ == "__main__":
    sys.exit(main())