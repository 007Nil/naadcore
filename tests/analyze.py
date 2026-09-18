#!/usr/bin/env python3
"""WAV analysis for NaadCore harmonium renders.

Usage:
  analyze.py <audio.wav> [timing.txt]

Reports global peak/RMS plus per-note segment stats: onset time (10%->90%
of steady-state RMS), release time (note-off to -60 dB), sustained amplitude
modulation rate (FFT of the RMS envelope in 0.1-5 Hz), peak/RMS.

timing.txt (optional): one note per line, "start_s end_s label" (tab or
space separated). Without it, segments are auto-detected by silence
thresholding and release is measured from the detected sustain end.
"""

import sys
import wave
import numpy as np

HOP = 256
AM_BAND = (0.1, 5.0)
SILENCE_DB = -60.0


def read_wav(path):
    with wave.open(path, "rb") as w:
        sr = w.getframerate()
        n = w.getnframes()
        raw = w.readframes(n)
        width = w.getsampwidth()
    if width == 2:
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 4:
        x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise SystemExit(f"unsupported sample width: {width}")
    if w.getnchannels() == 2:
        x = x.reshape(-1, 2).mean(axis=1)
    return sr, x


def rms_envelope(x, sr):
    frames = len(x) // HOP
    env = np.sqrt((x[: frames * HOP].reshape(frames, HOP) ** 2).mean(axis=1))
    return env, frames / sr  # env values + frame duration in seconds


def db(x):
    return 20.0 * np.log10(max(x, 1e-12))


def steady_rms(env, t0, t1, fs):
    a, b = max(0, int(t0 * fs)), min(len(env), int(t1 * fs))
    return float(np.median(env[a:b])) if b > a else float(np.max(env) or 1e-12)


def onset_time(env, fs, seg_start, steady):
    a = int(seg_start * fs)
    ref = max(steady, 1e-12)
    lo, hi = None, None
    for i in range(a, len(env)):
        if lo is None and env[i] >= 0.1 * ref:
            lo = i
        if env[i] >= 0.9 * ref:
            hi = i
            break
    if lo is None or hi is None:
        return float("nan")
    return (hi - lo) / fs


def release_time(env, fs, off_time, steady, floor):
    a = int(off_time * fs)
    if a >= len(env):
        return float("nan")
    # -60 dB below steady, clamped to the file noise floor (s16 renders
    # bottom out around -90 dBFS)
    threshold = max(steady * 10 ** (-60.0 / 20.0), floor * 2.0)
    for i in range(a, len(env)):
        if env[i] <= threshold:
            return (i - a) / fs
    return float("nan")  # never reached the threshold before end of file


def am_stats(env, fs, t0, t1):
    a, b = max(0, int(t0 * fs)), min(len(env), int(t1 * fs))
    seg = env[a:b]
    if len(seg) < 16:
        return float("nan"), float("nan")
    depth = db(np.percentile(seg, 95)) - db(np.percentile(seg, 5))
    trend = np.polyfit(np.arange(len(seg)), seg, 1)
    detrended = seg - np.polyval(trend, np.arange(len(seg)))
    spec = np.abs(np.fft.rfft(detrended * np.hanning(len(seg))))
    freqs = np.fft.rfftfreq(len(seg), d=1.0 / fs)
    band = (freqs >= AM_BAND[0]) & (freqs <= AM_BAND[1])
    if not band.any():
        return float("nan"), float(depth)
    idx = np.argmax(spec[band])
    return float(freqs[band][idx]), float(depth)


def auto_segments(env, fs):
    thr = 10 ** (SILENCE_DB / 20.0)
    active = env > thr
    segs = []
    i = 0
    n = len(active)
    while i < n:
        if active[i]:
            j = i
            last = i
            while j < n:
                if active[j]:
                    last = j
                elif (j - last) / fs > 0.25:
                    break
                j += 1
            if (last - i) / fs > 0.08:
                segs.append((i / fs, last / fs))
            i = j
        else:
            i += 1
    return segs


def load_timing(path):
    segs = []
    with open(path) as f:
        for line in f:
            line = line.split("#")[0].strip()
            parts = line.split()
            if len(parts) < 2:
                continue
            segs.append((float(parts[0]), float(parts[1]),
                         parts[2] if len(parts) > 2 else ""))
    return segs


def load_offset(wav_path):
    """Live captures write a '<wav>.offset' sidecar holding the seconds
    between recorder start and MIDI playback start."""
    try:
        with open(wav_path + ".offset") as f:
            return float(f.read().strip())
    except (OSError, ValueError):
        return 0.0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    sr, x = read_wav(sys.argv[1])
    env, _ = rms_envelope(x, sr)
    fs = sr / HOP
    total = len(x) / sr

    peak_db = db(np.abs(x).max())
    rms_db = db(np.sqrt((x ** 2).mean()))
    floor = float(env.min())
    print(f"file: {sys.argv[1]}")
    print(f"duration: {total:.2f} s   peak: {peak_db:.1f} dBFS   "
          f"rms: {rms_db:.1f} dBFS   floor: {db(floor):.0f} dBFS")

    if len(sys.argv) > 2:
        offset = load_offset(sys.argv[1])
        if offset:
            print(f"applying capture offset: {offset:+.3f} s")
            notes = [(s + offset, e + offset, lbl)
                     for s, e, lbl in load_timing(sys.argv[2])]
        else:
            notes = load_timing(sys.argv[2])
    else:
        notes = [(s, e, "auto") for s, e in auto_segments(env, fs)]

    print(f"{'label':>12s} {'start':>7s} {'end':>7s} {'peak_dB':>8s} "
          f"{'rms_dB':>7s} {'onset_ms':>9s} {'rel_ms':>8s} "
          f"{'AM_Hz':>6s} {'AM_dB':>6s}")
    for t0, t1, label in notes:
        a, b = int(t0 * sr), min(len(x), int(t1 * sr))
        if b <= a:
            continue
        seg = x[a:b]
        steady = steady_rms(env, t0 + 0.15, t1 - 0.05, fs)
        onset = onset_time(env, fs, t0, steady)
        rel = release_time(env, fs, t1, steady, floor)
        rate, depth = am_stats(env, fs, t0 + 0.2, t1 - 0.1)
        print(f"{label:>12s} {t0:7.2f} {t1:7.2f} "
              f"{db(np.abs(seg).max()):8.1f} {db(np.sqrt((seg**2).mean())):7.1f} "
              f"{onset * 1000:9.1f} {rel * 1000:8.1f} "
              f"{rate:6.2f} {depth:6.1f}")


if __name__ == "__main__":
    main()
