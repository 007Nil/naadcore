#!/usr/bin/env python3
"""Generate the Phase 4 probe MIDI tracks (format 0, channel 0, 120 BPM).

Same hand-rolled VLQ/format-0 technique as gen_midi.py and the Phase 3
T8/T9 probes; committed tracks live in tests/midi/ next to this script.

Tracks:
  T10_duplicate_noteon  duplicate NoteOns while held (no re-trigger) +
                        duplicate release-proof (single release tail)
  T11_all_notes_off     chord + CC 123 (All Notes Off) mid-hold + a note
                        afterwards to prove clean state after CC 123
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

    def cc(self, t, controller, value, ch=0):
        self.events.append((self.at(t), self.seq,
                            bytes([0xB0 | ch, controller, value])))
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


def t10_duplicate_noteon():
    """Duplicate NoteOns on held keys: 60 (dup @40), then 64 (dup @70).

    Expected with the Phase 4 fix: NO onset transient at 2.5 s / 7.5 s and
    exactly ONE release tail per note (4.5 s / 9.5 s).
    """
    tr = Track()
    tr.note_on(0.5, 60, 100)
    tr.note_on(2.5, 60, 40)    # duplicate while held — must be ignored
    tr.note_off(4.5, 60)
    tr.note_on(6.0, 64, 100)
    tr.note_on(7.5, 64, 70)    # duplicate while held — must be ignored
    tr.note_off(9.5, 64)
    return tr.build(11.5)


def t11_all_notes_off():
    """Multi-channel CC 123 + cross-channel NoteOff proof.

    Notes held on channels 0/1/3, then CC 123 sent on channel 0 ONLY.
    Phase <=3 behavior would silence channel 0 and strand the ch1/ch3
    voices (infinite drone). Expected with the Phase 4 fix: ALL voices
    release at 3.0 s. Then note 72 sounds on ch2 at 5.0 s and is released
    by a NoteOff arriving on ch5 at 7.0 s (stored-channel release; a
    stranded voice would drone past 7.0 s).
    """
    tr = Track()
    tr.note_on(0.5, 60, 100, ch=0)
    tr.note_on(0.6, 64, 80, ch=1)
    tr.note_on(0.7, 67, 60, ch=3)
    tr.cc(3.0, 123, 0, ch=0)
    tr.note_on(5.0, 72, 90, ch=2)
    tr.note_off(7.0, 72, 0, ch=5)
    return tr.build(9.0)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "midi")
    os.makedirs(out_dir, exist_ok=True)
    tracks = {
        "T10_duplicate_noteon.mid": t10_duplicate_noteon,
        "T11_all_notes_off.mid": t11_all_notes_off,
    }
    for name, builder in tracks.items():
        path = os.path.join(out_dir, name)
        with open(path, "wb") as f:
            f.write(builder())
        print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()
