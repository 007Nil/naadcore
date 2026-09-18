#!/usr/bin/env python3
"""Generate the Phase 5 drone probe MIDI tracks (format 0, channel 0, 120 BPM).

Same hand-rolled VLQ/format-0 technique as gen_midi.py and the Phase 3/4
probes; committed tracks live in tests/midi/ next to this script.

Tracks:
  T12_drone_feature  melody phrases (with gaps) over a drone that sounds
                     for the WHOLE track plus a long drone-only tail —
                     render twice (drone=48,55 vs drone=off) and compare
  T13_drone_cc123    drone + melody + CC 123 mid-track: everything
                     (drone included) must hit the noise floor after the
                     CC 123, then the synth must still play notes
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


def t12_drone_feature():
    """Melody over a continuous drone + long drone-only tail.

    Melody: note 69 (A4) phrases at 100, spread 0.7-14.8 s with ~0.9 s
    gaps (drone-only windows between phrases). The last 6.2 s of the
    21 s track are drone-only (drone=48,55 render): long enough for the
    carrier-line FFT to resolve the double-stop detune beat (~0.3-0.5 Hz)
    on the drone notes. The drone itself comes from the render config
    (drone=48,55 vs drone=off) — the track is identical for both.
    """
    tr = Track()
    for start, end, note in [
        (0.7, 2.3, 69),
        (3.2, 4.8, 69),
        (5.7, 7.3, 71),
        (8.2, 9.8, 69),
        (10.7, 12.3, 67),
        (13.2, 14.8, 69),
    ]:
        tr.note_on(start, note, 100)
        tr.note_off(end, note)
    return tr.build(21.0)


def t13_drone_cc123():
    """Drone + melody + CC 123 mid-track.

    Render with drone=48,55: melody note 69 (0.5-2.5 s) over the drone;
    CC 123 at 3.0 s must release EVERYTHING (drone included — old CC 123
    behavior only cleared one channel); the 3.3-4.8 s window must sit at
    the noise floor; note 72 (5.0-7.0 s) proves the synth still plays
    after the reset. Restarting the drone after CC 123 needs a config
    change, which the renderer cannot do mid-run (deliberately) — that
    half is covered at state level by tests/test_plugin_config.cpp.
    """
    tr = Track()
    tr.note_on(0.5, 69, 100)
    tr.note_off(2.5, 69)
    tr.cc(3.0, 123, 0)
    tr.note_on(5.0, 72, 90)
    tr.note_off(7.0, 72)
    return tr.build(9.0)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "midi")
    os.makedirs(out_dir, exist_ok=True)
    tracks = {
        "T12_drone_feature.mid": t12_drone_feature,
        "T13_drone_cc123.mid": t13_drone_cc123,
    }
    for name, builder in tracks.items():
        path = os.path.join(out_dir, name)
        with open(path, "wb") as f:
            f.write(builder())
        print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()
