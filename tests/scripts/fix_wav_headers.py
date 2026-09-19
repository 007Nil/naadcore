#!/usr/bin/env python3
"""Fix WAV files whose declared RIFF/data sizes exceed the actual bytes.

yt-dlp's -x --audio-format wav pipe sometimes writes a data-chunk size
larger than the data it actually wrote (~tens of KB short). sox then
fails with "premature EOF" and produces empty output. This rewrites the
data chunk size (and RIFF size) to the actual file length.

Usage: fix_wav_headers.py <file.wav> [more.wav ...]
Writes <name>_fixed.wav next to each input.
"""
import struct
import sys


def fix(path: str) -> None:
    b = bytearray(open(path, "rb").read())
    if bytes(b[:4]) != b"RIFF" or bytes(b[8:12]) != b"WAVE":
        print(f"{path}: not a RIFF/WAVE file, skipping")
        return
    pos, data_hdr = 12, None
    while pos + 8 <= len(b):
        cid = bytes(b[pos:pos + 4])
        sz = struct.unpack("<I", bytes(b[pos + 4:pos + 8]))[0]
        if cid == b"data":
            data_hdr = pos
            break
        pos += 8 + sz + (sz & 1)
    if data_hdr is None:
        print(f"{path}: no data chunk found, skipping")
        return
    actual = len(b) - (data_hdr + 8)
    declared = struct.unpack("<I", bytes(b[data_hdr + 4:data_hdr + 8]))[0]
    if declared <= actual:
        print(f"{path}: declared size OK ({declared} <= {actual}), no fix needed")
        return
    b[data_hdr + 4:data_hdr + 8] = struct.pack("<I", actual)
    b[4:8] = struct.pack("<I", len(b) - 8)
    out = path.replace(".wav", "_fixed.wav")
    open(out, "wb").write(b)
    print(f"{path}: declared {declared} -> actual {actual}, wrote {out}")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        fix(p)
