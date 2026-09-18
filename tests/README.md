# NaadCore Harmonium Realism — Test Harness

Phase 0 measurement harness for the harmonium realism work. Everything here
is reproducible from scripts; generated audio (tests/renders/) and reference
clips (tests/references/) are gitignored.

## Layout

```
tests/
├── midi/                  # 7 base test tracks (T1–T7, format 0,
│                          #   120 BPM, ch 0) + T8–T11 Phase 3/4 probes
│                          #   (T10/T11 via gen_probes_phase4.py) — in git
├── scripts/
│   ├── gen_midi.py        # regenerates tests/midi/ (pure stdlib, no mido)
│   ├── render_sf2.sh      # offline FluidSynth render of a track
│   ├── capture_live.sh    # live capture through CLI -> plugin -> audio
│   ├── render_plugin.cpp  # plugin-in-loop offline renderer (ad hoc)
│   ├── run_render_plugin.sh # compiles+runs render_plugin.cpp (ad hoc)
│   ├── derive_sf2.py      # Phase 3 SF2 surgery: builds harmonium_v2.sf2
│   │                      #   (double-reed detuned preset) from the original
│   ├── am_spectrum.py     # Phase 3 AM-band spectrum: top AM components per
│   │                      #   note segment (separates beat from in-sample AM)
│   ├── gen_probes_phase4.py # Phase 4 probes: T10 (duplicate NoteOn),
│   │                      #   T11 (multi-channel CC 123 + cross-ch off)
│   ├── sf2_audit.py       # SF2 binary structure dump (Phase 1 audit)
│   └── run_config_tests.sh# compiles+runs test_plugin_config.cpp (ad hoc,
│                          #   not wired into the project CMake build)
├── analyze.py             # numpy WAV analysis (onset/release/AM/peak/RMS)
├── test_plugin_config.cpp # config-seam tests (gain/reverb/chorus +
│                          #   attack_ms/release_ms + stop + coupler/
│                          #   sub_octave live keys, 147 checks)
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

# 6. Config-seam unit tests (gain/reverb/chorus/attack_ms/release_ms/stop)
tests/scripts/run_config_tests.sh

# 7. Offline render through the REAL plugin (no audio hardware needed;
#    reflects voicing, envelope generators, bellows velocity, reed stops —
#    which render_sf2.sh cannot). Real-time: ~1.15x track length.
tests/scripts/run_render_plugin.sh tests/midi/T1_single_note_envelope.mid \
    tests/renders/my_plugin_render.wav 3

# 7b. Same, in the double-reed stop (any plugin config key can be passed
#     as trailing KEY=VALUE pairs, applied via set_config before init)
tests/scripts/run_render_plugin.sh tests/midi/T1_single_note_envelope.mid \
    tests/renders/my_plugin_render_double.wav 3 stop=double
```

Timing files are plain text: `start_s end_s label` per line (see
tests/timings/ for the ones used in RESULTS.md). Live captures write a
`<wav>.offset` sidecar (seconds between recorder start and playback start)
that analyze.py applies automatically.

## Which capture path works on this machine

**The PipeWire/pulse path works.** Details:

1. The plugin's internal default is the ALSA audio driver. PipeWire's ALSA
   plugin (`pipewire-alsa`) proxies the CLI's ALSA output, so a stream
   appears in `pactl list short sink-inputs`. (The CLI's `--audio-driver`
   flag IS wired through since 2026-09-18, but FluidSynth's native
   `pipewire` driver fails on this machine — missing `pw_init()` — so the
   default ALSA path remains the working capture route.)
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

## Plugin-in-loop offline rendering (Phase 2, 2026-09-18)

`tests/scripts/render_plugin.cpp` (run via `tests/scripts/run_render_plugin.sh`)
closes the gap that `render_sf2.sh` could never reflect plugin behavior: it
dlopens the actual `libharmonium_plugin.so` (same mechanism as
PluginManager), initializes it with the FluidSynth **"file" audio driver**,
parses the MIDI file itself (format 0/1, tempo-mapped, running status), and
feeds each event to the plugin at its file time over the wall clock (the
file driver renders in real time), then renames the driver's `fluidsynth.wav`
output to the requested path.

- Usage: `tests/scripts/run_render_plugin.sh <track.mid> <out.wav> [tail_s]`
- A 20 s track takes ~23 s wall clock (real-time render).
- Caveats: needs a writable CWD-relative output directory (the harness
  chdirs there for the driver's default `fluidsynth.wav`); no `dlclose()` at
  exit (same FluidSynth/GLib teardown crash as test_plugin_config.cpp);
  event timing lands within a few ms of nominal (driver timer jitter) —
  fine at analyze.py's 5.8 ms resolution.
- Validated on T1/T4 against the live captures: peak levels identical,
  release behavior matches (see RESULTS.md Phase 2 section).

## Phase 3: derived SoundFont + reed stops (2026-09-19)

`tests/scripts/derive_sf2.py` builds the in-repo double-reed font from the
original (which is never modified):

```bash
python3 tests/scripts/derive_sf2.py \
    /home/nil/harmonium-companion/harmonium.sf2 \
    plugins/harmonium/soundfonts/harmonium_v2.sf2 4 "harmonium double"
```

- Args: `<in.sf2> <out.sf2> [detune_cents (default 4)] [preset_name]`.
- The committed `plugins/harmonium/soundfonts/harmonium_v2.sf2` was generated
  with the defaults above (+4 cents). Preset 0 is byte-identical behavior to
  the original font; preset 1 duplicates every key zone with fineTune=+4.
- Regenerating: the script only needs the original font; sample data is
  referenced, so the derived file grows by <1 KB.
- Validate with `python3 tests/scripts/sf2_audit.py
  plugins/harmonium/soundfonts/harmonium_v2.sf2` (expect 2 presets /
  2 instruments, doubled zones with `fineTune=4`) and
  `printf 'load <font>\ninst <font-id>\nquit\n' | fluidsynth`.

Config overrides in the plugin-in-loop renderer (Phase 3):

```bash
tests/scripts/run_render_plugin.sh tests/midi/T1_single_note_envelope.mid \
    tests/renders/my_stop_double.wav 3 stop=double
```

Any number of `KEY=VALUE` pairs after the tail argument is applied via
`plugin->set_config()` before `init()` (the pre-init storage path), so every
plugin config key is renderable: `stop=double`, `attack_ms=15`,
`soundfont_path=/path/to.sf2`, ... A rejected pair aborts the render.

`render_sf2.sh` now defaults to the in-repo derived font; set
`HARMONIUM_SOUNDFONT=/path/to.sf2` to use any other font.

Phase 3 probe tracks:

| Track | Purpose |
|---|---|
| T8_program_change_probe | program change 1 + 4 s note 60 — proves PROGRAM_CHANGE is ignored (single-carrier spectrum in stop=single despite PC 1 in the file) |
| T9_beat_probe | notes 43/60/79 held 6 s each — fine AM resolution (~0.2 Hz bins) for beat-rate measurement (use with `am_spectrum.py`) |

`tests/scripts/am_spectrum.py <wav> <timing.txt> [n_peaks] [pad0] [pad1]`
lists the strongest AM components per note segment in the 0.1–5 Hz band
(RMS-envelope FFT) — use it to separate the detune beat from the in-sample
~3 Hz beating (RESULTS.md Phase 3 has the numbers).

## Phase 4: layer router probes (2026-09-19)

Regenerate the two Phase 4 probe tracks with
`python3 tests/scripts/gen_probes_phase4.py` (same hand-rolled format-0
writer as gen_midi.py; committed tracks are in tests/midi/):

| Track | Purpose |
|---|---|
| T10_duplicate_noteon | duplicate NoteOns on held keys (60@100 then 60@40; 64@100 then 64@70) — with the Phase 4 fix there is NO onset transient at 2.5 s / 7.5 s and exactly ONE release tail per note (4.5 s / 9.5 s) |
| T11_all_notes_off | notes held on channels 0/1/3, CC 123 sent on channel 0 ONLY at 3.0 s (old behavior strands ch1/ch3 voices as an infinite drone), then note 72 on ch2 at 5.0 s released by a NoteOff on ch5 at 7.0 s (cross-channel release; old behavior strands the ch2 voice) |

Render + check (plugin-in-loop; renderer MIDI supports any channel, which
is what makes T11 a real multi-channel proof):

```bash
tests/scripts/run_render_plugin.sh tests/midi/T10_duplicate_noteon.mid \
    tests/renders/p4_T10.wav 2
python3 tests/analyze.py tests/renders/p4_T10.wav   # onsets at 0.5/6.0 only
```

Layer probes (layers are config keys — pass them to the renderer):

```bash
tests/scripts/run_render_plugin.sh tests/midi/T3_chord_uniformity.mid \
    tests/renders/p4_T3_coupler_on.wav 3 coupler=on
tests/scripts/run_render_plugin.sh tests/midi/T1_single_note_envelope.mid \
    tests/renders/p4_T1_sub_on.wav 3 sub_octave=on
```

Objective Phase 4 numbers and the measurement method (power-subtraction:
layer power = P(on-render) − P(off-render) in matched mid-sustain windows)
are in tests/RESULTS.md. WAV times include the renderer's 0.2 s lead-in.


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
| T8_program_change_probe | (Phase 3) program-change ignore proof, single 4 s note 60 |
| T9_beat_probe | (Phase 3) 6 s notes 43/60/79 for beat-rate measurement |
| T10_duplicate_noteon | (Phase 4) duplicate NoteOn while held → no re-attack, one release tail |
| T11_all_notes_off | (Phase 4) multi-channel CC 123 + cross-channel NoteOff → nothing stranded |

T8–T11 are hand-generated probe tracks (small inline Python writers, same
VLQ/format-0 technique as gen_midi.py; T10/T11 via
`scripts/gen_probes_phase4.py`) — they are committed.
