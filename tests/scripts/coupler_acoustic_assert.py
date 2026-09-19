#!/usr/bin/env python3
"""Coupler acoustic assertion: is the octave-up voice audible in the render?

Usage: coupler_acoustic_assert.py <coupler_off.wav> <coupler_on.wav>

Compares matched mid-sustain windows of two plugin-in-loop renders of
tests/midi/T16_coupler_acoustic.mid (single note 60): the OFF baseline and
the ON render (coupler=on applied pre-init — the same state the CLI's
"coupler on" gives to NEW presses). The coupler voice is note 72 on FluidSynth
channel 15, fixed gain CC 7 = 60, +3 cents detuned, so its partials sit at
~2*f0*2^(3/1200)*n — interleaved with, but slightly above, the main voice's
even partials.

Assertions (all must hold; exits 0 on PASS, 1 on FAIL):
  1. PRESENCE — total sustain power grows by at least MIN_PRESENCE_DB
     (a layer voice was actually added) and at most MAX_PRESENCE_DB
     (it is a ~-7..-9 dB layer, not a second full voice).
  2. PLACEMENT — of the per-bin clipped added spectral power, at least
     (1 - MAX_LOW_FRACTION) must live above 1.4*f0, where the octave voice
     lives. The 2026-09-19 subsonic-tuning bug (constant +3 cents passed to
     fluid_synth_activate_key_tuning, which expects ABSOLUTE cents = 100*key)
     put essentially ALL added power below 260 Hz as an ~8 Hz-tuned rumble:
     audible-spectrum placement is what catches that failure class. Per-bin
     clipping is essential: the coupler's partials sit only +3 cents from the
     main's even partials, so raw band-power differences suffer coherent
     cancellation (a band can SHRINK although the voice was added).
  3. OCTAVE LINES — in the coupler's 2nd-partial zone (just above 4*f0,
     where the fixed render gains its own cluster), at least MIN_GROW_BINS
     FFT bins must grow by >= MIN_LINE_GROWTH_DB over the OFF render.

Numbers are printed for the record (tests/RESULTS.md).
"""

import sys
import wave
import numpy as np

# Analysis window (seconds, in WAV time; the plugin-in-loop renderer has a
# 0.2 s lead-in, note-on at 0.7 s, note-off at 3.7 s, release 200 ms).
WIN = (1.2, 3.4)

# Thresholds (measured on the fixed build 2026-09-19 with wide margins:
# presence +0.50 dB, low fraction ~0.0001, line growth +14..+38 dB at 5+
# bins; the broken build measured low fraction ~1.0 and zero line growth).
MIN_PRESENCE_DB = 0.15
MAX_PRESENCE_DB = 1.5
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


def window_power(x, sr):
    a, b = int(WIN[0] * sr), int(WIN[1] * sr)
    seg = x[a:b]
    return float((seg ** 2).mean())


def spectrum(x, sr):
    a, b = int(WIN[0] * sr), int(WIN[1] * sr)
    seg = x[a:b] * np.hanning(b - a)
    spec = np.abs(np.fft.rfft(seg)) ** 2
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    return freqs, spec


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    sr_off, x_off = read_wav(sys.argv[1])
    sr_on, x_on = read_wav(sys.argv[2])
    if sr_off != sr_on:
        print(f"FAIL: sample rates differ ({sr_off} vs {sr_on})")
        return 1

    p_off = window_power(x_off, sr_off)
    p_on = window_power(x_on, sr_on)
    presence_db = 10.0 * np.log10(p_on / p_off)

    freqs, s_off = spectrum(x_off, sr_off)
    _, s_on = spectrum(x_on, sr_on)

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
    # fundamental (2*f0, +3 cents) hides inside the main's 2nd-partial
    # beating cluster, but its 2nd partial (~4*f0, +6 cents at that
    # frequency) pokes out as a NEW cluster next to the main's 4th.
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
            f"no layer voice was added (coupler noteon path broken?)")
    if presence_db > MAX_PRESENCE_DB:
        failures.append(
            f"presence {presence_db:+.2f} dB > {MAX_PRESENCE_DB} dB — "
            f"the layer is far too loud (layer gain / routing wrong?)")
    if low_fraction > MAX_LOW_FRACTION:
        failures.append(
            f"added power is {100*low_fraction:.1f}% below "
            f"{LOW_SPLIT_FACTOR*f0:.0f} Hz — the added voice is NOT the "
            f"octave (subsonic/wrong-pitch tuning bug class; see "
            f"apply_coupler_detune in harmonium_plugin.cpp)")
    if grow_bins < MIN_GROW_BINS:
        failures.append(
            f"only {grow_bins} bins grew >= {MIN_LINE_GROWTH_DB} dB in the "
            f"coupler 2nd-partial zone (need {MIN_GROW_BINS}) — no octave-up "
            f"line cluster appeared")

    if failures:
        for f in failures:
            print("FAIL:", f)
        return 1
    print("PASS: coupler octave-up voice is audible in the ON render "
          f"(presence {presence_db:+.2f} dB, {100*(1-low_fraction):.2f}% of "
          f"added power above {LOW_SPLIT_FACTOR*f0:.0f} Hz, {grow_bins} "
          f"octave-zone bins grew)")
    return 0


if __name__ == "__main__":
    sys.exit(main())