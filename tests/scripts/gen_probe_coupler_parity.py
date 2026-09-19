#!/usr/bin/env python3
"""Generate the coupler parity probe track (format 0, ch 0, 120 BPM).

Same hand-rolled VLQ/format-0 technique as gen_midi.py and the Phase 3/4/5
probe generators; the committed track lives in tests/midi/.

Tracks:
  T17_coupler_parity  notes 48, 60, 72, 84 held 3 s each (1 s gaps), vel 100.
                        Rendered twice by tests/scripts/check_coupler_parity.sh
                        (coupler=off vs coupler=on pre-init). The consecutive
                        pairs (48,60), (60,72), (72,84) are exactly the
                        main-voice / octave-voice pairs of the representative
                        notes 48/60/72: in the OFF render the fundamental line
                        of note N+12 IS the level the coupler voice of note N
                        reproduces at CC 7 = 100 (same preset, same channel
                        volume, same velocity — parity by construction), so
                        the OFF-render line comparison measures the octave-vs-
                        main level without the spectral overlap of a coupled
                        render. The ON/OFF power comparison measures what the
                        coupler actually adds (tests/scripts/
                        coupler_parity_measure.py prints both).
"""

import os
import struct

TPQN = 480
TEMPO_US = 500000


class Track:
    """Collects (tick, seq, bytes) events and writes a format-0 .mid file."""

    def __init__(self):
        self.events = []
        self.seq = 0

    def at(self, seconds):
        return int(round(seconds * TPQN * 1_000_000 / TEMPO_US))

    def note_on(self, t, note, vel, ch=0):
        self.events.append((self.at(t), self.seq, bytes([0x90 | ch, note, vel])))
        self.seq += 1

    def note_off(self, t, note, vel=0, ch=0):
        self.events.append((self.at(t), self.seq, bytes([0x80 | ch, note, vel])))
        self.seq += 1

    def build(self, duration_s):
        self.events.sort(key=lambda e: (e[0], e[1]))
        meta = b"\xff\x51\x03" + TEMPO_US.to_bytes(3, "big")
        sig = b"\xff\x58\x04\x04\x02\x18\x08"
        track = bytearray()
        track += _vlq(0) + meta
        track += _vlq(0) + sig
        last = 0
        for tick, _, data in self.events:
            track += _vlq(tick - last) + data
            last = tick
        track += _vlq(max(0, self.at(duration_s) - last)) + b"\xff\x2f\x00"
        header = struct.pack(">HHH", 0, 1, TPQN)
        return b"MThd" + struct.pack(">I", len(header)) + header \
            + b"MTrk" + struct.pack(">I", len(track)) + bytes(track)


def _vlq(value):
    out = bytearray([value & 0x7F])
    value >>= 7
    while value:
        out.insert(0, 0x80 | (value & 0x7F))
        value >>= 7
    return bytes(out)


# (on_time_s, off_time_s, note): 3 s holds, 1 s gaps, 4 segments.
SEGMENTS = [(0.5, 3.5, 48), (4.5, 7.5, 60), (8.5, 11.5, 72),
            (12.5, 15.5, 84)]


def t17_coupler_parity():
    """Notes 48/60/72/84 held 3 s each — the coupler parity probe."""
    tr = Track()
    for on, off, note in SEGMENTS:
        tr.note_on(on, note, 100)
        tr.note_off(off, note)
    return tr.build(17.0)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "midi")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "T17_coupler_parity.mid")
    with open(path, "wb") as f:
        f.write(t17_coupler_parity())
    print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()