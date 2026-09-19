#!/usr/bin/env python3
"""Generate the coupler acoustic-check probe track (format 0, ch 0, 120 BPM).

Same hand-rolled VLQ/format-0 technique as gen_midi.py and the Phase 3/4/5
probe generators; the committed track lives in tests/midi/.

Tracks:
  T16_coupler_acoustic  single note 60 @ vel 100 held 0.5-3.5 s. Rendered
                        twice by tests/scripts/check_coupler_acoustic.sh
                        (coupler=off vs coupler=on pre-init); the analyzer
                        asserts the octave-up voice (note+12 on ch 15) is
                        AUDIBLE in the on-render: added sustain power must
                        live at the octave (the 2026-09-19 subsonic-tuning
                        bug put it all below 260 Hz as an ~8 Hz rumble) and
                        the coupler's 2nd-partial cluster must grow.
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


def t16_coupler_acoustic():
    """One note 60 held 3 s — the coupler octave A/B probe."""
    tr = Track()
    tr.note_on(0.5, 60, 100)
    tr.note_off(3.5, 60)
    return tr.build(5.0)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "midi")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "T16_coupler_acoustic.mid")
    with open(path, "wb") as f:
        f.write(t16_coupler_acoustic())
    print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()