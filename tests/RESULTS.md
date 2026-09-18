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
