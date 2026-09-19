# NaadCore Harmonium Realism — Test Harness

Phase 0 measurement harness for the harmonium realism work. Everything here
is reproducible from scripts; generated audio (tests/renders/) and reference
clips (tests/references/) are gitignored.

## Layout

```
tests/
├── midi/                  # 7 base test tracks (T1–T7, format 0,
│                          #   120 BPM, ch 0) + T8–T13 Phase 3/4/5 probes
│                          #   (T10/T11 via gen_probes_phase4.py,
│                          #   T12/T13 via gen_probes_phase5.py) — in git
├── scripts/
│   ├── gen_midi.py        # regenerates tests/midi/ (pure stdlib, no mido)
│   ├── render_sf2.sh      # offline FluidSynth render of a track
│   ├── capture_live.sh    # live capture through CLI -> plugin -> audio
│   ├── render_plugin.cpp  # plugin-in-loop offline renderer (ad hoc)
│   ├── run_render_plugin.sh # compiles+runs render_plugin.cpp (ad hoc)
│   ├── derive_sf2.py      # Phase 3/6 SF2 surgery: builds harmonium_v2.sf2
│   │                      #   (double-reed detuned preset) and, with --click,
│   │                      #   harmonium_v3.sf2 (+ key-click sample/instrument/
│   │                      #   preset 2) from the original
│   ├── am_spectrum.py     # Phase 3 AM-band spectrum: top AM components per
│   │                      #   note segment (separates beat from in-sample AM)
│   ├── gen_probes_phase4.py # Phase 4 probes: T10 (duplicate NoteOn),
│   │                      #   T11 (multi-channel CC 123 + cross-ch off)
│   ├── gen_probes_phase5.py # Phase 5 probes: T12 (drone feature),
│   │                      #   T13 (drone + CC 123)
│   ├── sf2_audit.py       # SF2 binary structure dump (Phase 1 audit)
│   └── run_config_tests.sh# compiles+runs test_plugin_config.cpp (ad hoc,
│                          #   not wired into the project CMake build)
├── e2e_cli_coupler.sh    # E2E: CLI runtime coupler (stdin commands → plugin;
│                          #   wipes ./build and rebuilds first, asserts the
│                          #   exact coupler output sequence, EOF-alive
│                          #   behavior, and banner-vs-HEAD build id)
├── analyze.py             # numpy WAV analysis (onset/release/AM/peak/RMS)
├── test_plugin_config.cpp # config-seam tests (gain/reverb/chorus +
│                          #   attack_ms/release_ms + stop + coupler/
│                          #   sub_octave + drone/drone_level + key_click/
│                          #   variation + audio_device keys, 287 checks)
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
| sox (SoX_ng 14.7.0.9) | OK (installed 2026-09-19) — trimming, normalization, spectrograms |
| ffmpeg | missing (not needed for the current workflow) |
| yt-dlp (2026.03.17) | OK (installed 2026-09-19) — reference-clip downloads |

yt-dlp notes: YouTube per-video downloads intermittently fail with
`HTTP Error 403: Forbidden` (bot protection / rate limiting) — retry with
different search terms or use archive.org, which worked reliably. yt-dlp's
wav pipe can write a data-chunk size larger than the actual bytes; sox then
fails with "premature EOF" and empty output — fix with
`python3 tests/scripts/fix_wav_headers.py <file.wav>` before trimming.
Never run sox in-place (`sox f f`) — it truncates the file to zero; use a
temp output and rename.

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

# 6. Config-seam unit tests (all keys: gain/reverb/chorus/attack_ms/
#    release_ms/stop/coupler/sub_octave/drone/drone_level/key_click/
#    variation/audio_device)
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

**Capture caveat with `--audio-device` (Phase A):** a direct hardware
device (`hw:`/`plughw:`) passed to the CLI bypasses PipeWire entirely, so
no sink-input appears in the PipeWire graph and `parecord
--monitor-stream` capture (capture_live.sh) cannot see the stream — for
captures either omit `--audio-device`, use `default`, or use the
pulseaudio driver with the sink name. (Since Phase B the launcher's device
menu warns about exactly this when offering its hw:/plughw: options — for
captures pick `default` there too.)

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
committed upstream copy `plugins/harmonium/soundfonts/harmonium_original.sf2`
(which is never modified; that path is also the script's default input):

```bash
python3 tests/scripts/derive_sf2.py \
    plugins/harmonium/soundfonts/harmonium_original.sf2 \
    plugins/harmonium/soundfonts/harmonium_v2.sf2 4 "harmonium double"
```

- Args: `[in.sf2] <out.sf2> [detune_cents (default 4)] [preset_name]
  [--click]` — the input defaults to the in-repo provenance copy
  `harmonium_original.sf2` (resolved from the script's own location, so it
  works from any CWD).
- The committed `plugins/harmonium/soundfonts/harmonium_v2.sf2` was generated
  with the defaults above (+4 cents). Preset 0 is byte-identical behavior to
  the original font; preset 1 duplicates every key zone with fineTune=+4.
- Regenerating: the script only needs the original font; reed sample data is
  referenced, so the derived file grows by <1 KB.
- Validate with `python3 tests/scripts/sf2_audit.py
  plugins/harmonium/soundfonts/harmonium_v2.sf2` (expect 2 presets /
  2 instruments, doubled zones with `fineTune=4`) and
  `printf 'load <font>\ninst <font-id>\nquit\n' | fluidsynth`.

## Phase 6: key-click font + click/variation usage (2026-09-19)

`derive_sf2.py --click` additionally emits the click layer into the output
font — the committed `plugins/harmonium/soundfonts/harmonium_v3.sf2` (now the
build default) was generated with:

```bash
python3 tests/scripts/derive_sf2.py \
    plugins/harmonium/soundfonts/harmonium_original.sf2 \
    plugins/harmonium/soundfonts/harmonium_v3.sf2 --click
```

- Adds a synthesized 40 ms "KeyClick" sample (numpy; brown-noise burst,
  FFT-bandpassed 700–4000 Hz, −8 dBFS peak, 2/15 ms fades — all constants at
  the top of derive_sf2.py; an audition WAV is written to
  /tmp/opencode/keyclick_22050.wav) and preset 2 "key click": one wide zone
  (keys 21–108, keynum=60, no loop) with a SELF-ENDING envelope (attack
  1 ms / hold 0 / decay 40 ms / sustain 1000 cB = fully closed / release
  15 ms). The click voice ends ≤21 ms after onset — a self-sustaining click
  would be a permanent drone (see tests/RESULTS.md Phase 6 for the proof).
- `sf2_audit.py` on v3: 3 presets / 3 instruments / 15 samples. Preset 0 is
  byte-identical to v2 (verified: T1 offline renders through the
  deterministic fluidsynth CLI compare equal, 0 LSB).

Click + variation usage (config keys, so also usable as renderer overrides):

```bash
# faint keyboard chirp on every note onset:
tests/scripts/run_render_plugin.sh tests/midi/T4_staccato_repeat.mid \
    tests/renders/p6_T4_low.wav 3 key_click=low
# clearly audible tick:
tests/scripts/run_render_plugin.sh tests/midi/T1_single_note_envelope.mid \
    tests/renders/p6_T1_high.wav 1 key_click=high
# humanized repeats (default) vs byte-exact Phase 5 behavior:
tests/scripts/run_render_plugin.sh tests/midi/T4_staccato_repeat.mid \
    tests/renders/p6_T4_varon.wav 3                  # variation=on default
tests/scripts/run_render_plugin.sh tests/midi/T4_staccato_repeat.mid \
    tests/renders/p6_T4_plain.wav 3 variation=off key_click=off
```

Measurement notes: the plugin-in-loop renderer is wall-clock throttled, so
two runs of the same config differ in event-to-block placement (renderer
placement jitter, NOT plugin nondeterminism) — per-note segment peaks are
timing-robust and are the right determinism metric (same config → same
per-note pattern to 0.000 dB). For exact sample-level click measurements use
the deterministic fluidsynth CLI: the click is exactly reproducible by a
MIDI file that selects preset 2 on channel 12, sets CC 7 = 64 and plays the
same velocities the plugin does (tests/RESULTS.md Phase 6 has the numbers).

Config overrides in the plugin-in-loop renderer (Phase 3):

```bash
tests/scripts/run_render_plugin.sh tests/midi/T1_single_note_envelope.mid \
    tests/renders/my_stop_double.wav 3 stop=double
```

Any number of `KEY=VALUE` pairs after the tail argument is applied via
`plugin->set_config()` before `init()` (the pre-init storage path), so every
plugin config key is renderable: `stop=double`, `attack_ms=15`,
`soundfont_path=/path/to.sf2`, ... A rejected pair aborts the render.

`render_sf2.sh` now defaults to the in-repo Phase 6 derived font
(harmonium_v3.sf2); set `HARMONIUM_SOUNDFONT=/path/to.sf2` to use any other
font.

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

## Phase 5: drone probes (2026-09-19)

Regenerate the two Phase 5 probe tracks with
`python3 tests/scripts/gen_probes_phase5.py` (same hand-rolled format-0
writer as gen_midi.py/gen_probes_phase4.py; committed tracks are in
tests/midi/):

| Track | Purpose |
|---|---|
| T12_drone_feature | melody phrases (note 69/71/67, gaps between) + a long tail — render twice (`drone=48,55` vs `drone=off`): drone lines must be present through the whole on-render and absent in the off render; melody lines must be identical |
| T13_drone_cc123 | melody + drone + CC 123 at 3.0 s — everything (drone included) hits the noise floor after the CC 123; note 72 afterwards proves the synth still plays |

Render + analyze (the drone itself is config — pass it to the renderer;
a 7 s tail gives a ~6 s drone-only window at the end of T12):

```bash
tests/scripts/run_render_plugin.sh tests/midi/T12_drone_feature.mid \
    tests/renders/p5_T12_drone_on.wav 7 drone=48,55
tests/scripts/run_render_plugin.sh tests/midi/T12_drone_feature.mid \
    tests/renders/p5_T12_drone_off.wav 7 drone=off
tests/scripts/run_render_plugin.sh tests/midi/T13_drone_cc123.mid \
    tests/renders/p5_T13.wav 2 drone=48,55
python3 tests/analyze.py tests/renders/p5_T13.wav   # no segment between the CC123 and note 72
```

Drone usage examples (config keys, so also usable as renderer overrides):

```bash
# Sa+Pa drone under everything, slightly quieter than the default:
tests/scripts/run_render_plugin.sh tests/midi/T6_repertoire_phrase.mid \
    tests/renders/p5_T6_drone.wav 3 drone=48,55 drone_level=35
# drone in the double-reed stop (drone voices use the double preset too):
tests/scripts/run_render_plugin.sh tests/midi/T5_drone_plus_melody.mid \
    tests/renders/p5_T5_drone_double.wav 3 drone=48,55 stop=double
```

Objective Phase 5 numbers (carrier-line FFT method, drone level vs melody,
CC 123 floor) are in tests/RESULTS.md. Line-level measurement used ad-hoc
numpy carrier-line FFTs (Hanning window, ±2 Hz line windows) on the raw
waveform — the same technique as the Phase 3 beat table.


## CLI runtime coupler E2E harness (2026-09-19)

`bash tests/e2e_cli_coupler.sh` (runs from anywhere; resolves the repo root
from its own location):

1. `rm -rf build` + cmake configure + full rebuild — it never tests a
   stale binary.
2. Pipes `status / coupler on / status / coupler off / status` into the
   real CLI + real plugin (`--audio-driver file`, no hardware needed) under
   `timeout 10`, then asserts the lines `Coupler: off` → `Coupler ON` →
   `Coupler: on` → `Coupler OFF` → `Coupler: off` appear in order AND the
   process was killed by timeout (exit 124), not self-exiting.
3. Runs the CLI with `</dev/null` stdin: the EOF message
   (`stdin closed (EOF) - CLI commands disabled, Ctrl+C to exit`) must
   print exactly once and the process must stay alive until the timeout
   kill (no busy-spin, no exit).
4. Sanity: the `naadcore-cli build <id>` startup banner's commit id must
   equal `git rev-parse --short HEAD`.

Passes all checks as of 2026-09-19 (build id 4ce9f50).

## Reference-clip workflow

Reference harmonium recordings go in `tests/references/` (gitignored —
**personal use only, never commit them**). Prepared A/B package
(2026-09-19, see `tests/RESULTS.md`):

| Clip | Source | Trim | Content |
|---|---|---|---|
| `ref_refA.wav` | yt-dlp "harmonium solo close mic" | 0–15 s | solo phrases |
| `ref_refB.wav` | same | 33–48 s | solo phrases (second take) |
| `ref_scaleA.wav` | archive.org harmonium item | 8–28 s | sustained scale work |

All peak-normalized to −3 dBFS. Matching current-build renders (also
peak-normalized) live in `tests/renders/ab/`:
`T1_single_note_envelope_current.wav`, `T2_scale_legato_current.wav`,
`T6_repertoire_phrase_current.wav` (+ `*_spec.png` spectrograms for each
clip/render pair).

Reproducing the package:

```bash
cd tests/references
yt-dlp -x --audio-format wav -o 'ref_raw.%(ext)s' 'ytsearch1:harmonium solo close mic'
yt-dlp -x --audio-format wav -o 'ref_scale_raw.%(ext)s' 'https://archive.org/...'  # archive.org items
python3 ../scripts/fix_wav_headers.py ref_raw.wav ref_scale_raw.wav
sox ref_raw_fixed.wav ref_fixed.wav && sox ref_scale_raw_fixed.wav ref_scale_fixed.wav
sox ref_fixed.wav ref_refA.wav trim 0 15          # pick active regions via RMS scan
sox ref_fixed.wav ref_refB.wav trim 33 15
sox ref_scale_fixed.wav ref_scaleA.wav trim 8 20
for f in ref_refA ref_refB ref_scaleA; do
    sox "$f.wav" "${f}_n.wav" gain -n -3 && mv "${f}_n.wav" "$f.wav"
done

# current-build renders + normalization + spectrograms
for t in T1_single_note_envelope T2_scale_legato T6_repertoire_phrase; do
    ../scripts/run_render_plugin.sh ../midi/$t.mid ../renders/ab/${t}_current.wav
done
for f in ../renders/ab/*_current.wav; do
    sox "$f" "${f%.wav}_n.wav" gain -n -3 && mv "${f%.wav}_n.wav" "$f"
    sox "$f" -n spectrogram -o "${f%.wav}_spec.png" -x 900 -y 500
done
sox ../../tests/references/ref_refA.wav -n spectrogram -o ../renders/ab/ref_refA_spec.png -x 900 -y 500
```

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
| T12_drone_feature | (Phase 5) melody over a continuous drone + drone-only tail (drone=48,55 vs off comparison) |
| T13_drone_cc123 | (Phase 5) drone + melody + CC 123 → everything to the floor, synth still alive |

T8–T13 are hand-generated probe tracks (small inline Python writers, same
VLQ/format-0 technique as gen_midi.py; T10/T11 via
`scripts/gen_probes_phase4.py`, T12/T13 via `scripts/gen_probes_phase5.py`)
— they are committed.
