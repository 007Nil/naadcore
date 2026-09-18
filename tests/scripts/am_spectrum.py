#!/usr/bin/env python3
"""Phase 3 AM-band spectrum analysis: top AM components per note segment.

For each timing segment, computes the FFT of the detrended, Hann-windowed
RMS envelope over the sustained part and lists the N strongest peaks in the
0.1-5 Hz band with their relative level (dB re: strongest). Used to separate
the in-sample ~3 Hz beating from the new slow detune beat in stop=double.

Usage: am_spectrum.py <wav> <timing.txt> [n_peaks] [sustain_start] [sustain_end]
"""
import sys
import wave
import numpy as np


def read_wav(path):
    with wave.open(path, "rb") as w:
        sr = w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()),
                          "<i2").astype(float) / 32768.0
        if w.getnchannels() == 2:
            x = x.reshape(-1, 2).mean(axis=1)
    return sr, x


def main():
    sr, x = read_wav(sys.argv[1])
    timing = []
    with open(sys.argv[2]) as f:
        for line in f:
            line = line.split("#")[0].strip()
            parts = line.split()
            if len(parts) >= 2:
                timing.append((float(parts[0]), float(parts[1]), parts[2]
                               if len(parts) > 2 else "seg"))
    n_peaks = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    pad0 = float(sys.argv[4]) if len(sys.argv) > 4 else 0.3
    pad1 = float(sys.argv[5]) if len(sys.argv) > 5 else 0.15
    hop = 256
    fs = sr / hop
    print(f"AM spectrum (top {n_peaks} peaks in 0.1-5 Hz), "
          f"envelope fs={fs:.1f} Hz, window = sustained part of segment")
    for t0, t1, label in timing:
        a = int((t0 + pad0) * sr)
        b = int((t1 - pad1) * sr)
        frames = (b - a) // hop
        env = np.sqrt((x[a:a + frames * hop].reshape(frames, hop) ** 2)
                      .mean(axis=1))
        env = env - np.polyval(np.polyfit(np.arange(len(env)), env, 1),
                               np.arange(len(env)))
        spec = np.abs(np.fft.rfft(env * np.hanning(len(env))))
        freqs = np.fft.rfftfreq(len(env), 1.0 / fs)
        band = (freqs >= 0.1) & (freqs <= 5.0)
        f, s = freqs[band], spec[band]
        # local maxima only
        peaks = []
        for i in range(1, len(f) - 1):
            if s[i] > s[i - 1] and s[i] >= s[i + 1]:
                peaks.append((f[i], s[i]))
        peaks.sort(key=lambda p: -p[1])
        top = peaks[:n_peaks]
        ref = top[0][1]
        parts = ", ".join(f"{fr:.2f} Hz ({20*np.log10(am/ref):+.1f} dB)"
                          for fr, am in top)
        print(f"  {label:>12s}: {parts}")


if __name__ == "__main__":
    main()
