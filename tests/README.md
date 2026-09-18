# NaadCore Harmonium Realism — Test Harness

Phase 0 measurement harness for the harmonium realism work. Everything here
is reproducible from scripts; generated audio (tests/renders/) and reference
clips (tests/references/) are gitignored.

## Layout

```
tests/
├── midi/                  # 7 generated test tracks (format 0, ch 0, 120 BPM,
│                          #   no CCs, no program changes) — tracked in git
├── scripts/
│   ├── gen_midi.py        # regenerates tests/midi/ (pure stdlib, no mido)
│   ├── render_sf2.sh      # offline FluidSynth render of a track
│   ├── capture_live.sh    # live capture through CLI -> plugin -> audio
│   ├── sf2_audit.py       # SF2 binary structure dump (Phase 1 audit)
│   └── run_config_tests.sh# compiles+runs test_plugin_config.cpp (ad hoc,
│                          #   not wired into the project CMake build)
├── analyze.py             # numpy WAV analysis (onset/release/AM/peak/RMS)
├── test_plugin_config.cpp # config-seam tests (gain/reverb/chorus live keys)
├── renders/               # rendered/captured WAVs (gitignored)
├── references/            # personal-use reference clips (gitignored)
├── RESULTS.md             # A/B score sheet + objective measurements
└── README.md              # this file
```

## Tool status on this machine (nil-ThinkPad-T470, 2026-09-18)

| Tool | Status |
|---|---|
| fluidsynth CLI | OK (2.4.8) |
| alsa-utils (aplaymidi, arecord, aseqsend, aconnect) | OK |
| pulseaudio-utils (parecord, pactl) | OK (PipeWire with pipewire-alsa) |
| python3 + numpy | OK (numpy 2.3.5) |
| mido | missing — gen_midi.py writes MIDI bytes directly |
| sox | missing (no sudo available); not required — fluidsynth writes WAV directly and analyze.py uses stdlib wave + numpy |
| ffmpeg | missing (no sudo available); only needed for reference-clip prep |
| yt-dlp | missing; only needed for reference-clip downloads |

sox/ffmpeg/yt-dlp could not be installed non-interactively (`sudo` needs a
TTY). They are only needed for the reference-clip workflow below; the core
harness (generate → render → capture → analyze) works without them.

## Reproducing everything

```bash
cd /home/nil/Projects/Personal/naadcore
cmake -B build && cmake --build build -j4     # if not already built

# 1. Regenerate the MIDI tracks
python3 tests/scripts/gen_midi.py

# 2. Offline baselines (stock FluidSynth defaults = pre-Phase-1 voicing)
for t in tests/midi/T*.mid; do
    tests/scripts/render_sf2.sh "$t" \
        "tests/renders/baseline_phase0_sf2_$(basename "${t%.mid}").wav" baseline
done

# 3. Phase-1 offline renders (pinned plugin voicing)
for t in tests/midi/T*.mid; do
    tests/scripts/render_sf2.sh "$t" \
        "tests/renders/phase1_sf2_$(basename "${t%.mid}").wav"
done

# 4. Live capture (full CLI -> plugin -> audio chain)
tests/scripts/capture_live.sh tests/midi/T1_single_note_envelope.mid

# 5. Analyze (auto-segments, or pass a timing file for exact notes)
python3 tests/analyze.py tests/renders/baseline_phase0_sf2_T1_single_note_envelope.wav
python3 tests/analyze.py tests/renders/<capture>.wav <timing.txt>

# 6. Config-seam unit tests (gain/reverb/chorus live keys)
tests/scripts/run_config_tests.sh
```

Timing files are plain text: `start_s end_s label` per line (see
tests/timings/ for the ones used in RESULTS.md). Live captures write a
`<wav>.offset` sidecar (seconds between recorder start and playback start)
that analyze.py applies automatically.

## Which capture path works on this machine

**The PipeWire/pulse path works.** Details:

1. The plugin's internal default is the ALSA audio driver (the CLI's
   `--audio-driver` flag is parsed but NOT wired through — known dead-flag
   bug). PipeWire's ALSA plugin (`pipewire-alsa`) proxies the CLI's ALSA
   output, so a stream appears in `pactl list short sink-inputs`.
2. `capture_live.sh` snapshots sink-inputs, starts the CLI (subscribed to
   the Midi Through port so no keyboard is needed), finds the new
   sink-input index, and captures it with `parecord --monitor-stream=<idx>`.
3. The track is played with `aplaymidi -p <naadcore input port>` (the port
   the CLI registers, found via `aconnect -l`).
4. Validated end-to-end on T1 (Phase 0 plugin):
   `tests/renders/20260918_T1_single_note_envelope.wav` — onsets 23–58 ms,
   releases 46–58 ms, sustain AM ~3 Hz, matching the offline render.
   Re-validated after Phase 1 (gain 0.4/reverb/chorus-off voicing) with
   `tests/renders/20260918_T1_phase1_live.wav` — peak levels match the
   phase1 offline render exactly (-25.7 dBFS for note 60 v100).

The pure-ALSA snd-aloop fallback in the script (modprobe snd-aloop +
`~/.asoundrc` redirect + `arecord -D plughw:Loopback,1,0`) was NOT needed
here and is untested on this machine (modprobe needs root).

## Reference-clip workflow

Reference harmonium recordings go in `tests/references/` (gitignored —
**personal use only, never commit them**). Workflow when yt-dlp/sox are
available:

```bash
yt-dlp -x --audio-format wav -o tests/references/ref_raw.wav '<url>'
# trim to a representative 15–30 s phrase:
sox tests/references/ref_raw.wav tests/references/ref.wav trim <start> <dur>
# normalize loudness so A/B listening is fair:
sox tests/references/ref.wav tests/references/ref_norm.wav gain -n -3
# (or: ffmpeg -i ref.wav -af loudnorm=I=-16:TP=-1.5 ref_norm.wav)
```

Score reference vs old vs new per track in `tests/RESULTS.md`.

## Track catalog

| Track | Purpose |
|---|---|
| T1_single_note_envelope | attack/reed speech, sustain, release at 2 velocities + 2 registers |
| T2_scale_legato | legato phrasing, uniform-bellows velocity hand-off |
| T3_chord_uniformity | chord evenness with staggered velocities, per-member release |
| T4_staccato_repeat | retrigger behavior, repeated-note chatter |
| T5_drone_plus_melody | drone stability with melody above |
| T6_repertoire_phrase | musical realism: bhajan chords, grace notes, sustained Sa |
| T7_velocity_sweep | velocity → amplitude/timbre mapping |
