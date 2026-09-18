#!/usr/bin/env python3
"""Generate the NaadCore harmonium test MIDI tracks (format 0, channel 0).

All tracks: 120 BPM, no CCs, no program changes. Output goes to
tests/midi/ next to this script. Pure stdlib — no mido required.

Tracks:
  T1_single_note_envelope  isolated notes for envelope measurement
  T2_scale_legato          legato scale phrases (uniform-bellows check)
  T3_chord_uniformity      block chords with staggered velocities
  T4_staccato_repeat       fast retriggering
  T5_drone_plus_melody     held drone with melody above
  T6_repertoire_phrase     bhajan-style chord progression with ornaments
  T7_velocity_sweep        one note at six velocities
"""

import os
import struct

TPQN = 480  # ticks per quarter note
TEMPO_US = 500000  # 120 BPM


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

    def note(self, t, dur, note, vel, ch=0):
        self.note_on(t, note, vel, ch)
        self.note_off(t + dur, note, 0, ch)

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


def t1_single_note_envelope():
    """Isolated notes at different velocities and registers (~23 s)."""
    tr = Track()
    t = 0.0
    for note, vel in [(60, 100), (60, 40), (43, 100), (79, 100)]:
        tr.note(t, 4.0, note, vel)
        t += 5.0  # 4 s hold + 1 s gap
    return tr.build(t + 3.0)  # trailing tail for release measurement


def t2_scale_legato():
    """Ascending/descending scale phrases with 60 ms legato overlap (~31 s).

    Phrase A starts at velocity 100, later notes 64; phrase B starts at 40.
    The sequence A B A B exercises the uniform-bellows reference and the
    baton pass between phrases.
    """
    tr = Track()
    up = [60, 62, 64, 65, 67, 69, 71, 72, 74, 76]
    down = list(reversed(up))[1:-1] + [60]  # 76..62,60 without re-hitting 76
    scale = up + down
    step = 0.34  # 400 ms notes, 60 ms overlap
    t = 0.0
    for phrase_first_vel in [100, 40, 100, 40]:
        for i, note in enumerate(scale):
            vel = phrase_first_vel if i == 0 else 64
            tr.note(t + i * step, 0.4, note, vel)
        t += (len(scale) - 1) * step + 0.4 + 1.0  # phrase + 1 s gap
    return tr.build(t + 1.0)


def t3_chord_uniformity():
    """Block chords with per-member velocity stagger; one release per second."""
    tr = Track()
    chord = [(60, 100), (64, 60), (67, 30), (72, 90)]
    t = 0.0
    for first_vel in [100, 30]:
        for i, (note, vel) in enumerate(chord):
            v = first_vel if i == 0 else vel
            tr.note_on(t + i * 0.01, note, v)
        for i, (note, _) in enumerate(chord):
            tr.note_off(t + 5.0 + i, note)
        t += 10.0  # 8 s chord + 2 s gap
    return tr.build(t + 1.0)


def t4_staccato_repeat():
    """Fast staccato retriggering: repeated note, then another, then alternation."""
    tr = Track()
    t = 0.0
    for note in [60, 67]:
        for _ in range(10):
            tr.note(t, 0.12, note, 100)
            t += 0.24
    for i in range(60):  # alternating 60/64, 30 pairs
        tr.note(t, 0.12, 60 if i % 2 == 0 else 64, 100)
        t += 0.24
    return tr.build(t + 1.0)


def t5_drone_plus_melody():
    """Held 48+55 drone for 16 s with a melody playing above."""
    tr = Track()
    for note in [48, 55]:
        tr.note(0.0, 16.0, note, 100)
    melody = [67, 69, 71, 72, 71, 69, 67]
    for i, note in enumerate(melody):
        tr.note(1.0 + i * 2.0, 1.6, note, 90)
    return tr.build(19.0)


def t6_repertoire_phrase():
    """Bhajan-style 8 bars of C-F-G-C (2 s per bar) with passing melody,
    two grace-note ornaments, and a final sustained Sa (~18 s)."""
    tr = Track()
    chords = {
        "C": [60, 64, 67],
        "F": [65, 69, 72],
        "G": [67, 71, 74],
    }
    progression = ["C", "F", "G", "C", "C", "F", "G", "C"]
    for bar, name in enumerate(progression):
        t0 = bar * 2.0
        for note in chords[name]:
            tr.note(t0, 2.0, note, 90)

    # Passing melody above the chords
    tr.note(0.5, 1.2, 72, 100)                       # bar 1
    tr.note(2.5, 0.8, 69, 100)                       # bar 2
    tr.note(3.4, 0.5, 72, 100)
    tr.note(4.62, 0.06, 73, 100)                     # grace ornament 1
    tr.note(4.70, 1.1, 74, 100)                      # bar 3
    tr.note(6.5, 0.6, 71, 100)                       # bar 4
    tr.note(7.2, 0.6, 72, 100)
    tr.note(8.5, 0.9, 67, 100)                       # bar 5
    tr.note(9.6, 0.9, 69, 100)
    tr.note(10.5, 1.0, 72, 100)                      # bar 6
    tr.note(12.5, 0.9, 71, 100)                      # bar 7
    tr.note(13.4, 0.06, 74, 100)                     # grace ornament 2
    tr.note(13.46, 0.6, 74, 100)

    # Final sustained Sa (low + high C) over the last chord and beyond
    tr.note(14.0, 4.0, 60, 100)
    tr.note(14.0, 4.0, 72, 100)
    return tr.build(18.5)


def t7_velocity_sweep():
    """Note 60 at velocities 20..127, each isolated (1 s on, 1 s gap)."""
    tr = Track()
    t = 0.0
    for vel in [20, 40, 60, 80, 100, 127]:
        tr.note(t, 1.0, 60, vel)
        t += 2.0
    return tr.build(t + 2.0)


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "midi")
    os.makedirs(out_dir, exist_ok=True)
    tracks = {
        "T1_single_note_envelope.mid": t1_single_note_envelope,
        "T2_scale_legato.mid": t2_scale_legato,
        "T3_chord_uniformity.mid": t3_chord_uniformity,
        "T4_staccato_repeat.mid": t4_staccato_repeat,
        "T5_drone_plus_melody.mid": t5_drone_plus_melody,
        "T6_repertoire_phrase.mid": t6_repertoire_phrase,
        "T7_velocity_sweep.mid": t7_velocity_sweep,
    }
    for name, builder in tracks.items():
        path = os.path.join(out_dir, name)
        with open(path, "wb") as f:
            f.write(builder())
        print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()
