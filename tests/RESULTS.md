# Harmonium Realism — A/B Results

Score sheet for the realism work. Scale 1–5 (1 = clearly synthetic, 5 =
indistinguishable from a good harmonium recording). "old" = pre-Phase-1
plugin voicing (stock FluidSynth defaults), "new" = current plugin voicing,
"ref" = personal-use reference clip (see tests/README.md).

## Subjective score sheet

Attributes:

- **A1** attack / reed speech
- **A2** sustain / shimmer (tremolo, beating)
- **A3** reed timbre across registers (low/mid/high evenness)
- **A4** chord uniformity
- **A5** legato phrasing
- **A6** release tail
- **A7** stops / coupler / drone behavior
- **A8** space / ambience (reverb, room feel)

| Track | Attribute | ref | old | new | Notes |
|---|---|---|---|---|---|
| T1 | A1 attack | | | | |
| T1 | A2 sustain | | | | |
| T1 | A3 registers | | | | |
| T1 | A6 release | | | | |
| T2 | A5 legato | | | | |
| T2 | A2 sustain | | | | |
| T3 | A4 chord uniformity | | | | |
| T3 | A6 release | | | | |
| T4 | A4 retrigger | | | | |
| T5 | A7 drone | | | | |
| T5 | A2 drone shimmer | | | | |
| T6 | A1+A5 musical phrasing | | | | |
| T6 | A7 ornaments/stops | | | | |
| T6 | A8 ambience | | | | |
| T7 | velocity response | | | | |

## Objective baseline (Phase 0 vs Phase 1 offline renders)

T1_single_note_envelope, notes at 4 s hold / 1 s gap. Measured with
`tests/analyze.py` on `tests/renders/baseline_phase0_sf2_T1_*.wav` (old:
stock FluidSynth defaults — gain 0.2, reverb room 0.5/damp 0.3/width
0.8/level 0.7, chorus on) and `tests/renders/phase1_sf2_T1_*.wav` (new:
gain 0.4, reverb room 0.2/damp 0.0/width 0.3/level 0.4, chorus off).

| Note | Render | peak dBFS | rms dBFS | onset ms | release ms | AM Hz | AM dB |
|---|---|---|---|---|---|---|---|
| 60 v100 | baseline (old) | -31.7 | -41.2 | 29.0 | 52.2 | 2.97 | 3.4 |
| 60 v100 | phase1 (new) | -25.7 | -35.2 | 29.0 | 58.0 | 2.97 | 3.4 |
| 60 v40 | baseline (old) | -47.6 | -57.1 | 34.8 | 34.8 | 2.97 | 3.5 |
| 60 v40 | phase1 (new) | -41.6 | -51.1 | 34.8 | 40.6 | 2.97 | 3.5 |
| 43 v100 | baseline (old) | -27.9 | -40.9 | 52.2 | 52.2 | 0.81 | 3.7 |
| 43 v100 | phase1 (new) | -21.9 | -34.8 | 52.2 | 58.0 | 0.81 | 3.7 |
| 79 v100 | baseline (old) | -32.1 | -42.0 | 23.2 | 52.2 | 3.25 | 6.2 |
| 79 v100 | phase1 (new) | -26.1 | -36.0 | 23.2 | 58.0 | 3.25 | 6.2 |

Phase 1 effect is exactly as designed: +6 dB overall level (gain 0.2→0.4),
unchanged attack/sustain character (onsets and AM identical — voicing does
not touch the samples), marginally longer measured release readings
(+5–6 ms, the modest reverb tail), and chorus off removes the stock
doubling.

### T7 velocity ladder (peak dBFS)

| Velocity | baseline (old) | phase1 (new) |
|---|---|---|
| 20 | -59.4 | -53.5 |
| 40 | -47.6 | -41.6 |
| 60 | -40.5 | -34.4 |
| 80 | -35.5 | -29.5 |
| 100 | -31.7 | -25.7 |
| 127 | -27.5 | -21.5 |

Dynamic range vel 20→127 ≈ 32 dB (concave curve, the SF2 default
velocity→attenuation modulator). Amplitude-only velocity response — no
timbre change (no velocity layers/modulators in the SF2; see
docs/HARMONIUM_SF2_AUDIT.md).

Live-capture validation (T1 through CLI -> plugin -> PipeWire, Phase 0
plugin): onsets 23–58 ms, releases 46–58 ms, AM ~3 Hz — matches the
offline render within measurement resolution (capture:
`tests/renders/20260918_T1_single_note_envelope.wav`).

## Phase 2: runtime envelope shaping (attack_ms=10 / release_ms=200)

Phase 2 shapes the volume envelope at runtime via FluidSynth channel
generators (see HANDOVER.md "Volume-envelope shaping"); the SF2 file is
untouched. Baseline = the committed Phase 1 build (a3bf255) re-captured
through the same live pipeline on the same day (test provenance: the Phase 1
plugin was rebuilt from the commit via `git stash`, captures taken, stash
popped).

### set_gen calibration (raw FluidSynth 2.4.8, file driver, reverb off)

`fluid_synth_set_gen` values are additive offsets on the instrument zone's
values, unclamped — measured with tests/analyze.py against a note-off
timing file (nominal values from SF2 timecent semantics):

| Case | set_gen value | Nominal release | Measured |
|---|---|---|---|
| baseline (no set_gen) | — | 100 ms | 104.5 ms |
| release gen 0 | 0 | additive 100 ms / override 1000 ms | **99 ms → additive** |
| release +1200 | +1200 | additive 200 ms / override 2000 ms | **203 ms → additive** |
| release −3986 | −3986 | additive 10 ms / override 100 ms | **17 ms → additive** |
| attack offset +13200 | +13200 | 2 s if unclamped / 1 s if clamped | **onset 1.2 s → unclamped** |
| release offset +5986 | +5986 | 4 s | 3175 ms (floor-limited) ✓ |

Compensation in the plugin: `offset = desired_tc − font_base_tc`
(font bases: attack −12000 tc ≈ 1 ms, release −3986 tc = 100 ms).

### T1 single-note envelope (live captures, timing-file analysis)

| Note | Capture | peak dBFS | onset ms | release ms | AM Hz | AM dB |
|---|---|---|---|---|---|---|
| 60 v100 | phase1 baseline | -25.7 | 23.2 | 69.7 | 2.97 | 3.4 |
| 60 v100 | **phase2** | -25.7 | 29.0 | **121.9** | 2.97 | 3.5 |
| 60 v40 | phase1 baseline | -41.6 | 23.2 | 46.4 | 2.97 | 3.5 |
| 60 v40 | **phase2** | -41.6 | 29.0 | **81.3** | 2.97 | 3.5 |
| 43 v100 | phase1 baseline | -21.9 | 58.0 | 58.0 | 0.81 | 3.7 |
| 43 v100 | **phase2** | -21.9 | 52.2 | **121.9** | 0.81 | 3.8 |
| 79 v100 | phase1 baseline | -26.1 | 23.2 | 63.9 | 3.25 | 6.1 |
| 79 v100 | **phase2** | -26.1 | 17.4 | **121.9** | 3.25 | 6.1 |

Reading the numbers:

- Release tail lengthened 46–70 → 81–122 ms measured (release_ms=200).
  Measured < nominal because analyze.py's threshold rides the reverb noise
  floor (~−63 dB below steady in the live chain) and FluidSynth's dB decay
  is back-loaded; the reverb-off calibration above confirms the nominal
  200 ms exactly (203 ms measured). Well inside the 100–350 ms target —
  audible tail, not muddy.
- Onsets unchanged within measurement resolution (±6 ms analysis hop): the
  10 ms envelope attack is small next to the recorded reed transient
  (23–58 ms), which is what we wanted — softened click, no sluggishness.
- Peaks, RMS, AM rate/depth identical: sustain and levels untouched.

### T4 staccato repeat (240 ms note spacing, 120 ms note + 120 ms gap)

Custom analysis (peak per note slot vs dip in the note-off→next-onset gap):

| Metric | phase1 baseline | phase2 |
|---|---|---|
| Onsets detected (of 80 slots) | 80/80 | **80/80** |
| First 10 note-60 peaks (dBFS) | −33.1…−32.9 | −32.9…−33.1 |
| Mean dip between notes (dBFS) | −100.0 (noise floor) | −83.8 |
| Onset-vs-dip contrast mean/min (dB) | 65.6 / 63.2 | **49.4 / 46.2** |

All onsets remain clearly distinct; the 200 ms tail reaches the next onset
≈44 dB below the note peak (phase1 sat at the −90 dBFS s16 floor). No
smearing at 120 ms spacing — retriggering stays clean.

### Plugin-in-loop offline renders (stretch goal)

`tests/scripts/run_render_plugin.sh` renders through the actual plugin .so
(FluidSynth "file" driver). T1 phase2 plugin render: releases 279–314 ms,
peaks identical to live (−25.7/−41.6/−21.9/−26.1 dBFS), AM unchanged —
longer than the live numbers purely because the measurement floor sits
lower in the offline file (no capture-chain noise). Confirms the envelope
generators (applied before `sfload`) survive font loading and that offline
plugin verification is now possible for future phases.

Captures/renders: `tests/renders/20260918_{T1,T4}_phase{1,2}_live.wav`,
`20260918_{T1,T4}_phase2_pluginrender.wav` (gitignored).

## Phase 3: reed stops — 2-reed detuned layering (2026-09-19)

`stop=single` = preset 0 of the derived font (today's sound); `stop=double`
= preset 1, every key zone duplicated with `fineTune = +4 cents` (see
HANDOVER.md "Reed stops", docs/HARMONIUM_SF2_AUDIT.md Phase 3 disposition).
All renders below are **plugin-in-loop** (`run_render_plugin.sh`), font =
`plugins/harmonium/soundfonts/harmonium_v2.sf2` unless stated.

### Derived font + preset 0 equivalence

- `sf2_audit.py` on the derived font: 2 presets / 2 instruments; preset 0
  zones byte-identical; "harmonium double" has 14 doubled key-zone pairs
  (`fineTune=4`) + one global zone per instrument. File 6,617,820 bytes
  (+476 vs original — sample data referenced, not copied).
- `fluidsynth -F` offline render of T1, preset 0, original font vs derived
  font: **sample-exact, 0 LSB difference** (6.62 MB stereo compared).
- Plugin-in-loop T1, stop=single, v2 font vs original font (via
  `soundfont_path` config override): peaks/RMS/AM identical
  (−25.7/−41.6/−21.9/−26.1 dBFS, AM 2.97/2.97/0.81/3.25 Hz); onset
  differences (29↔34.8 ms etc.) are the real-time renderer's ±6 ms event
  jitter, not a font difference.

### T1 sustained notes: single vs double

| Note | stop | peak dBFS | rms dBFS | onset ms | rel ms | AM Hz | AM dB |
|---|---|---|---|---|---|---|---|
| 60 v100 | single | -25.7 | -35.4 | 29.0 | 307.7 | 2.97 | 3.6 |
| 60 v100 | **double** | -22.7 | -32.4 | 23.2 | 313.5 | 1.35 | 6.7 |
| 60 v40 | single | -41.6 | -51.3 | 29.0 | 278.6 | 2.97 | 3.6 |
| 60 v40 | **double** | -38.6 | -48.3 | 23.2 | 284.4 | 1.35 | 6.7 |
| 43 v100 | single | -21.9 | -35.0 | 58.0 | 307.7 | 0.81 | 3.8 |
| 43 v100 | **double** | -20.5 | -32.2 | 29.0 | 319.3 | 2.43 | 4.7 |
| 79 v100 | single | -26.1 | -36.1 | 23.2 | 307.7 | 3.25 | 6.4 |
| 79 v100 | **double** | -20.7 | -33.1 | 11.6 | 313.5 | 3.79 | 11.1 |

Two reeds sum to +1.4…+5.4 dB peaks (partial coherence), AM depth grows
(3.6→6.7 dB at note 60; 6.4→11.1 dB at note 79) and the AM argmax moves off
the in-sample rate — the slow beat is in. T9 (6 s notes, ~0.19 Hz bins)
resolves the components (`am_spectrum.py`):

| Note | single: top AM components | double: top AM components |
|---|---|---|
| 43 | 0.60, 3.00 (−6.4) | 0.60, 2.40 (−1.3), 1.40 (−4.8), 2.80 (−5.0), 1.80 (−6.0), 3.40 (−6.2) |
| 60 | 3.00, 0.40 (−2.7), 1.20 (−10.2) | 2.00, 1.20 (−0.3), 3.00 (−6.9), 0.40 (−8.9), 3.80 (−10.1) |
| 79 | 3.40, 1.60 (−17.4) | 3.80, 0.60 (−16.3), 1.60 (−22.7), 2.20 (−23.7) |

Honest caveat: the RMS-envelope FFT of a *sum* mixes the beat (and its
harmonics — each reed partial k beats at k·Δf) with the in-sample AM, so the
envelope spectrum is a comb, not a single clean beat line. The authoritative
beat measurement is the raw-waveform carrier-line FFT (two spectral lines
per note separated by the beat):

| Note | Font pitch (single) | Expected beat (+4¢) | Measured carriers (double) | Measured beat |
|---|---|---|---|---|
| 43 | 103.33 Hz | 0.24 Hz | not separated | < 0.19 Hz (bin limit — 6 s window cannot resolve 0.24 Hz; envelope comb confirms a slow component) |
| 60 | 277.04 Hz | 0.64 Hz | 276.96 / 277.59 Hz | **0.56 Hz** |
| 79 | 832.22 Hz | 1.92 Hz | 832.16 / 834.07 Hz | **1.85 Hz** |

Measured beats sit 0.5 bin below expected (effective detune 3.5–3.8¢ vs
requested 4¢ — within the FFT resolution). Target 0.3–2 Hz: hit at notes
60/79; note 43's 0.24 Hz beat is marginally below (audible as very slow
shimmer; raise D or clamp the low register if it matters — open).

### Pitch check

Detune is asymmetric by design (zones at 0¢ and +4¢, mean +2¢): note 60's
carriers average 277.28 Hz vs single's 277.04 Hz → **+1.5 cents** —
imperceptible, pitch stable. Side observation (pre-existing, both stops):
the font plays ~92–104 cents sharp vs A440 equal temperament (103.3/98.0,
277.0/261.6, 832.2/784.0) — the samples' own tuning, unchanged by Phase 3.

### PROGRAM_CHANGE policy proof (T8)

T8 contains a program change to program 1 before the note. Plugin-in-loop,
`stop=single`: spectrum shows a **single carrier** (276.9 Hz) — the program
change did NOT switch to the double preset. Same file with
`stop=double`: two carriers 0.5 Hz apart — config-controlled stops work.
(Also covered by 96/96 config tests: PC events are swallowed with
PLUGIN_OK, notes keep playing.)

### T2 legato / T4 staccato in double mode

| Metric | single | double |
|---|---|---|
| T2 peak / rms dBFS | −22.9 / −40.8 | −19.8 / −38.3 |
| T2 onsets / releases | 17.4 ms / 63.9 ms | 11.6 ms / 63.9 ms |
| T4 onsets detected | 80/80 | **80/80** |
| T4 mean note peak / dip | −28.2 / −55.7 dBFS | −24.1 / −52.3 dBFS |
| T4 contrast mean/min | 27.5 / 26.7 dB | **28.2 / 25.8 dB** |

No glitching, onsets as distinct as single mode, no voice-overflow messages
in any render log (2 voices per note, FluidSynth polyphony 256 default).
(Note: the Phase 2 doc's T4 contrast numbers used a different dip window;
the single-vs-double comparison here uses one consistent method.)

### Config tests / build

- `run_config_tests.sh`: **96/96** checks (was 76; +20 for the `stop` key:
  default, valid set/get, live switch, pre-init storage + apply at init,
  strict rejection, PC-ignored handling).
- Clean build (rm -rf build): **0 warnings** (-Wall -Wextra -Wpedantic).
- Live CLI smoke: startup logs show the in-repo font path and
  `Synth stop: single`. Full live capture skipped: `capture_live.sh` has no
  config plumb (cannot set `stop`); plugin-in-loop renders cover it.

Renders: `tests/renders/p3_*.wav` (gitignored).

## Phase 4: layer router + behavior fixes (2026-09-19)

Octave coupler (ch 15, note+12, CC7=60, +3¢ detuned) and sub-octave (ch 14,
note−12, CC7=40) as live config keys `coupler` / `sub_octave`; duplicate
NoteOn ignored; CC 123 on all 16 channels; cross-channel NoteOff. All
renders below are **plugin-in-loop** (`run_render_plugin.sh`), font =
`plugins/harmonium/soundfonts/harmonium_v2.sf2`, default voicing
(attack 10 ms / release 200 ms, reverb on).

**Method — power subtraction.** Layer voices are detuned (+3¢) and/or
pitch-shifted a full octave from the main voice, so their partials beat
against the main's and sum approximately incoherently over a sustained
window: `P_layer = P(on-render) − P(off-render)` in matched mid-sustain
windows (0.2 s renderer lead-in accounted for). The layer's level relative
to the main voice is `10·log10(P_layer / P_main)`.

### Layer gains (T1, single note, all voices at bellows velocity)

| Note (vel) | Coupler excess | Sub-octave excess |
|---|---|---|
| 60 (100) | −7.59 dB | −11.43 dB |
| 60 (40) | −7.70 dB | −11.44 dB |
| 43 (100) | −7.81 dB | −13.92 dB |
| 79 (100) | −6.35 dB | −22.25 dB |

Coupler ≈ 6–8 dB below main ("clearly audible, clearly quieter" — the
per-note coupler level relative to its own main voice is uniform within
~1.5 dB; note 79's sub sits lower because its sub sample (67) differs).
Note velocity does not affect the ratio (both voices share the bellows
reference — uniformity preserved).

### Octave placement (exact ±1 octave, spectral lines)

- Coupler of note 60: fundamental appears at **555.0 Hz** (= f0(60) 277.0 ×
  2^(3/1200) — the +3¢ coupler tuning is visible as a separate line beside
  the main's 2nd harmonic at 554.0 Hz). Same for note 43 (207.0 Hz).
- Sub-octave of note 60: line at **138.4 Hz** (= f0/2, note 48) at −74 dBFS
  in the sub render, absent (−127 dBFS) in the plain render. Sub of 79
  (note 67, 414.8 Hz) present at −77.7 dBFS (low, matching the −22 dB
  excess above).
- T3 chord (60/64/67/72 held): coupler lines at 555.0/699.2/833.4/1109.0 Hz
  all present in the on render (e.g. 699.2 at −55.5 dBFS vs −62.5 noise
  level of that region without the layer), i.e. exactly one octave above
  each chord member.

### Per-member release (T3, coupler on): no residual octave

- Decisive single-note check (T1): coupler excess −6.6 dB during hold →
  **−29.7 dB** (measurement floor, P_on = P_off exactly) after the note-off.
  Same for the sub layer and for T3 after all four members release.
- Line-level (T3, member 64 releasing at 6.2 s): BOTH the main 2nd-harmonic
  line (698.0 Hz) and the coupler line (699.2 Hz) drop to −116/−119 dBFS in
  6.6–8.1 s (from −62.9/−54.8 while held) — the coupler voice released with
  its member; no residual octave.
- Whole-chord excess windows right after staggered member releases (0.6 s
  windows) stay elevated — reverb tails of the just-released coupler voices
  (they decay over ~1 s with the reverb); the long-window and line-level
  checks above are the authoritative no-residual evidence.

### T5 drone + melody (coupler on)

- Drone-only window (14.8–16.0 s, melody long gone): coupler excess
  **−4.40 dB** (2 drone couplers vs 2 drone mains) — octave-up energy
  present on the drone.
- Melody **unaffected**: the melody-69 line (465.5 Hz, 3.2–4.4 s) measures
  **−57.2 dBFS in BOTH the coupler-on and coupler-off renders** —
  bit-identical placement, the layer router doesn't touch melody voices
  (they sound at the same bellows reference on the same channel as before).

### T10 duplicate NoteOn (the fix's audible signature)

RMS level in ±250 ms windows across each duplicate instant (old behavior
re-noteoned at the reference velocity = a second/restarted voice ≈ +3 dB
step; a true onset is a +55 dB step):

| Event | RMS before → after | Δ |
|---|---|---|
| 60 onset (0.7 s) | −90.3 → −33.8 dBFS | **+56.5 dB** |
| 60 duplicate (2.7 s) | −36.3 → −35.9 dBFS | **+0.3 dB** |
| 64 onset (6.2 s) | −90.3 → −35.6 dBFS | **+54.7 dB** |
| 64 duplicate (7.7 s) | −35.5 → −37.4 dBFS | **−2.0 dB** |

The ±2 dB wiggle at duplicates equals the note's own in-sample AM (envelope
peak-to-peak 2.5–3.8 dB over the hold) — no onset transient. Exactly ONE
release tail per note (each release reaches the −90.3 dBFS s16 floor within
0.4 s; no second tail).

### T11 multi-channel CC 123 + cross-channel NoteOff

Notes held on channels 0/1/3, CC 123 sent on channel 0 ONLY:

| Window | RMS |
|---|---|
| chord hold (2.5–3.15 s) | −31.0 dBFS |
| +0.3 s after CC 123 (3.5–3.8 s) | **−90.3 dBFS (s16 floor)** |
| note 72 on ch2 hold (5.4–6.0 s) | −35.5 dBFS |
| after NoteOff arriving on ch5 (7.4–7.7 s) | **−90.3 dBFS (floor)** |

All voices released by the one-channel CC 123 (old behavior: fluid_synth_cc
123 on ch0 strands ch1/ch3 as infinite drones — the classic stuck-note
failure), and the ch2 voice is released by the cross-channel NoteOff (old
behavior: stranded forever). The renderer feeds whatever channel the MIDI
file carries, so this is a true multi-channel renderer-level proof (the
base tracks T1–T9 are ch 0 only).

### Envelope gens survive program_select (channels 14/15)

With `release_ms=2000`, the coupler excess tracks the main's full 2 s
shaped tail (+0.2–0.7 s after release: −4.1 dB, i.e. coupler tail ≈ main
tail). If `program_select` had reset the channel generators, the coupler
(100 ms font release) would collapse ~1.9 s earlier — it does not.

### Config tests / build

- `run_config_tests.sh`: **147/147** checks (was 96; +51 for the `coupler`/
  `sub_octave` keys — defaults, valid set/get live + pre-init, strict
  rejection (case/trailing space/empty/junk), mid-phrase toggles
  interleaved between note events, duplicate-NoteOn swallow, cross-channel
  noteoffs, CC 123 reset, coupler range clamp at note 120).
- Clean build (rm -rf build): **0 warnings** (-Wall -Wextra -Wpedantic).
- Live CLI smoke (timeout 5): startup shows `Synth stop: single` and
  `Synth layers: coupler=off sub_octave=off (ch15=note+12 CC7=60,
  ch14=note-12 CC7=40)`, no errors, exit 124 (alive).

**Mid-phrase toggle honesty note:** the renderer applies config before the
run only; true mid-run toggling is verified at state level (config tests
interleave `set_config("coupler"/"sub_octave")` between
`handle_midi_event` calls — voices start/release without error and state
readback is correct). Audible mid-phrase verification would need a renderer
config-change mechanism, which was deliberately not invented (no MIDI
semantics in the harness).

Renders: `tests/renders/p4_*.wav` (gitignored).

## Phase 5: drone (unpika) + config registry (2026-09-19)

Drone = fixture notes on internal channel 13 (`drone` / `drone_level`
config keys), fixed velocity 100, loudness via CC 7, no bellows-model
interaction, no bend/CC11 mirroring; CC 123 clears it. Registry doc:
docs/HARMONIUM_CONFIG.md. All renders below are **plugin-in-loop**
(`run_render_plugin.sh`), font = harmonium_v2.sf2, default voicing.
T12 = melody phrases (69/71/69/69/67/69 @100, 0.7–14.8 s) over a drone
sounding the whole track; the 7 s render tail gives a 15.5–21.5 s
drone-only window.

**Method — carrier-line FFT.** Hanning-windowed raw-waveform FFT of the
segment, line amplitude reported at the font's pitch for each note
(drone 48 → 139.5 Hz, drone 55 → 207.5 Hz, melody 69 → 465.5 Hz, 71 →
522.0 Hz; font plays ~1 semitone sharp — pre-existing). Noise floor =
mean amplitude 5–10 Hz off the line.

### T12: drone presence, level, and melody untouched

Drone-on render (`drone=48,55`, default `drone_level=45`) vs drone-off:

| Line | window | drone on | drone off |
|---|---|---|---|
| drone 48 f0 (139.5 Hz) | phrase 1 (1.0–2.3 s) | −59.5 dBFS | −107.2 (noise) |
| drone 48 f0 | phrase 3 (5.9–7.2 s) | −60.2 | −102.3 (noise) |
| drone 48 f0 | phrase 6 (13.4–14.7 s) | −60.2 | −106.4 (noise) |
| drone 48 f0 | tail (15.5–21.5 s) | −60.1 | −240 (digital silence) |
| drone 55 f0 (207.5 Hz) | phrase 1 | −60.3 | −109.7 (noise) |
| drone 55 f0 | tail | −60.7 | −240 |
| melody 69 f0 (465.5 Hz) | phrase 1 | **−45.9** | **−45.9** |
| melody 69 f0 | phrase 6 | **−45.9** | **−45.9** |
| melody 71 f0 (522.0 Hz) | phrase 3 (5.7–7.3 s) | **−45.3** | **−45.3** |

- **Present throughout:** the drone lines sit at −59.5…−60.8 dBFS in every
  window of the on-render (first phrase to the tail, drift < 1.3 dB —
  fixture behavior), and are at the noise floor / digital silence in the
  off-render.
- **Melody unchanged:** the melody fundamental lines are IDENTICAL to the
  reported resolution (−45.9/−45.9, −45.9/−45.9, −45.3/−45.3) between the
  two renders — the drone never touches melody voices or the bellows model.
  Global stats: peak −24.0 (on) vs −26.1 dBFS (off), RMS −37.1 vs −37.6 —
  the +2.1 dB peak is ordinary waveform summing, not a gain interaction.
- **Level vs melody (the drone_level default decision):** drone
  fundamentals ≈ −60 dBFS vs melody fundamental ≈ −45.6 dBFS → **−14.4 dB
  per drone voice, −11.4 dB with both drone notes power-summed** — clearly
  under the melody at typical levels, no masking (melody lines identical).
  Default `drone_level=45` kept (CC7=45 is 1.9 dB hotter than the
  sub-octave's 40 but the drone's register sits a full octave-plus below
  the melody line measured here).

### T12 in the double stop (`stop=double drone=48,55`)

The drone channel plays the double preset too. Drone-only tail, carrier
lines: note 55 resolves the detuned pair **207.50 / 208.00 Hz** (0.50 Hz
beat; expected 0.48 Hz at +4¢) and note 48 shows the merged hump at
139.3–139.9 Hz (its 0.32 Hz beat is ~2 bins at the 6 s window's 0.167 Hz
resolution — widened plateau vs single's single-bin peak). No errors, no
voice-overflow messages.

### T13: drone + CC 123 (`drone=48,55`)

Melody 69 (0.5–2.5 s) over the drone; CC 123 at 3.0 s; note 72 at 5.0–7.0 s:

| Window | RMS |
|---|---|
| melody+drone hold (1.0–2.4 s) | −33.7 dBFS |
| **post-CC123 (3.4–4.8 s)** | **−90.3 dBFS (s16 floor)** |
| note 72 after reset (5.4–6.8 s) | −36.1 dBFS |
| after note 72 off (7.4–8.8 s) | −90.3 dBFS (floor) |

The drone is included in the reset (old one-channel CC 123 would have left
ch13 droning forever), and the synth still plays afterwards. Restarting the
drone after CC 123 needs a config change (the renderer can't do that
mid-run, deliberately) — verified at state level in the config harness
(re-issuing the same `drone=48,55` restarts the notes).

### Config tests / build / smoke

- `run_config_tests.sh`: **213/213** checks (was 147; +66 for the
  `drone`/`drone_level` keys — defaults, valid set/get, boundary notes
  0/127, 8-note cap, add/remove/keep semantics at state level, empty-string
  = off canonicalization, strict rejection (sargam junk, 128, negative,
  float, empty/comma/space/duplicate tokens, 9 notes, overflow), CC 123
  reset + same-spec restart, pre-init storage + apply + live changes).
- Clean build (rm -rf build): **0 warnings** (-Wall -Wextra -Wpedantic).
- Live CLI smoke (timeout 5): startup shows `Synth stop: single`,
  `Synth layers: …` and the new `Synth drone: off (ch13 CC7=45 vel=100)`,
  no errors, exit 124 (alive).

Renders: `tests/renders/p5_*.wav` (gitignored).

## Listening notes

(reference clips pending — see tests/README.md for the workflow)

## Findings so far

- SF2 audit (docs/HARMONIUM_SF2_AUDIT.md): releaseVolEnv is 100 ms in the
  font and attackVolEnv is default (~1 ms, instant) — the ~30–50 ms attack
  we measure is the recorded reed transient itself. Phase 2 addressed both
  at runtime (attack_ms=10, release_ms=200 defaults, live-adjustable).
- Sustain has ~3 Hz AM at 2.5–6 dB depth baked into the looped samples
  (natural reed beating) — no LFO/modulators exist in the font.
- No velocity layers or velocity modulators: velocity is amplitude-only
  (~32 dB range, vel 20→127).
- The top octave (keys 65–84) is a single F4 sample stretched up to +19
  semitones — expect timbre thinning up high (A3 in the score sheet).
- Velocity 20 renders around -53 dBFS peak (phase1) — quiet but cleanly
  above the s16 noise floor.
- Phase 3 (2026-09-19): derived font `harmonium_v2.sf2` — preset 0 is
  sample-exact vs the original font; the +4¢ doubled zones produce the
  expected slow beat (0.56 Hz @ note 60, 1.85 Hz @ note 79) on top of the
  in-sample ~3 Hz beating. Font plays ~1 semitone sharp vs A440 ET
  (pre-existing, both stops).
- Phase 4 (2026-09-19): layers (octave coupler / sub-octave) measured at
  −6.4…−7.8 dB and −11.4…−13.9 dB below the main voice (CC7=60/40 via
  FluidSynth's CC7→attenuation default modulator; note 79's sub is
  quieter, −22 dB). The +3¢ coupler detune is visible as a separate
  spectral line (555.0 Hz next to 554.0 for note 60) — subtle main-vs-octave
  beat confirmed working. FluidSynth's `program_select` does not reset
  channel generators (release_ms=2000 tail-tracking proof) nor CC 7 (layer
  gains persist across stop changes).
- Phase 5 (2026-09-19): the drone (ch13 fixture, CC7=45, fixed vel 100)
  measures ≈11–14 dB under the melody fundamental line with melody lines
  bit-identical on/off (no bellows/gain interaction); the double stop
  applies to the drone channel too (0.50 Hz resolved beat on drone note
  55); CC 123 takes the drone to the s16 floor along with everything else.
