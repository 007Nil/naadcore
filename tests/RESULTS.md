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

## A/B listening package (prepared 2026-09-19)

The reference clips and current-build renders are prepared and
loudness-matched (all peak-normalized to −3 dBFS) — the subjective rows
above are now fillable. Files:

- `tests/references/ref_refA.wav`, `ref_refB.wav` (solo phrases, 15 s each),
  `ref_scaleA.wav` (sustained scale work, 20 s) — sources and trim
  provenance in `tests/README.md`
- `tests/renders/ab/T1_single_note_envelope_current.wav`,
  `T2_scale_legato_current.wav`, `T6_repertoire_phrase_current.wav`
  (plugin-in-loop renders of the current build, default config)
- Spectrograms: `tests/renders/ab/*_spec.png` for every clip/render

Suggested listening protocol (use the A1–A8 attribute definitions above):

1. Same headphones/speakers, comfortable fixed volume across all files.
2. Per pair: reference → render → reference → render. Score each
   attribute 1–5 in the table above with a one-line note.
3. Pairs by attribute:
   - **A1/A2/A6** — `ref_refA` vs `T1_..._current` (single notes,
     attack/sustain/release character)
   - **A5/A3** — `ref_scaleA` vs `T2_..._current` (legato, registers)
   - **A4/A7/A8 gestalt** — `ref_refB` vs `T6_..._current` (musical phrase)
4. Optional: spectogram side-by-side (`*_spec.png`) for attack transients,
   beat striping, and release shape.
5. Known physical differences to expect (not bugs): the references are
   different microphones/rooms/instruments; our top octave is a stretched
   sample (Phase 7 candidate); our reverb is a small-room FluidSynth
   preset.

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

> **Correction (2026-09-19, later same day):** the COUPLER measurements in
> this section (layer-gain table column, "octave placement" lines, T3/T5
> coupler lines) were taken while the coupler's +3¢ tuning was broken —
> every coupler voice was tuned to ~8 Hz (subsonic rumble), so the "coupler
> excess" below is the rumble's power and the "coupler lines" are the main
> voice's own beating sidebands. The SUB-OCTAVE column and the T10/T11
> behavior proofs are unaffected (channel 14 carries no tuning). See
> "Coupler acoustic check — subsonic-tuning bug fix" above for the
> line-verified post-fix coupler (≈ −9 dB at CC7=60).

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

## Phase 6: key-click/chiff + per-note micro-variation (2026-09-19)

The polish phase. Two new config keys: `key_click` (off/low/high — faint
mechanical key noise on every accepted NoteOn, internal channel 12, font
preset 2) and `variation` (on/off — deterministic per-note velocity jitter).
All renders below are **plugin-in-loop** (`run_render_plugin.sh`), font =
`plugins/harmonium/soundfonts/harmonium_v3.sf2` (new build default), default
voicing — unless stated.

### Font v3 + preset 0 comparability

- `sf2_audit.py` on v3: 3 presets / 3 instruments / 15 samples (+terminal);
  new "KeyClick" sample 882 frames (40 ms @ 22050 Hz, no loop); click
  instrument = one zone keys 21–108, keynum=60, attack 1 ms / hold ~0 /
  decay 40 ms / sustain 1000 cB (fully closed) / release 15 ms. File
  6,619,754 bytes (+1934 vs v2 — the click sample IS copied, +882 frames +
  headroom, plus pdta records).
- **Presets 0/1 identical between v2 and v3:** fluidsynth-CLI offline
  renders of T1 (preset 0) with v2 vs v3 are **byte-identical (0 LSB)**.
- Refactor guard: regenerating v2 with the updated `derive_sf2.py` reproduces
  the committed v2 **byte-identically**.

### Click self-end proof (the critical correctness point)

sustainVolEnv is an attenuation (0 = hold full level FOREVER), so the click
envelope decays to a fully-closed sustain (1000 cB = 100 dB down); the
unlooped 40 ms sample also runs out of data. Raw fluidsynth-CLI render,
preset 2, note 60 vel 127 held 4 s:

| Metric | Value |
|---|---|
| Burst audible (RMS env > −80 dBFS) | 502.1 → 521.0 ms after onset (**≤21 ms ≪ 60 ms**) |
| 0.56–4.4 s of the 4 s hold | **−90.3 dBFS = s16 digital silence floor** |

A self-sustaining click (sustain left open) would have been a permanent
drone — spec failure; measured: the voice self-ends.

### Click level calibration (final constants)

The exact calls the plugin makes (ch12: preset 2, CC 7 = 64, click velocity
45/75) replayed through the **deterministic** fluidsynth CLI (byte-identical
run-to-run; reverb off, gain 0.4), reed-only render subtracted in the time
domain — the residual IS the click:

| Mode | Click 0–60 ms power | Click peak | vs reed onset (peak −27.2 dBFS) | Active > −70 dBFS | Residual after 60 ms |
|---|---|---|---|---|---|
| low (vel 45) | −71.6 dBFS | −51.5 dBFS | **24.4 dB below** | 3.8–11.7 ms | **−240 dBFS = exact zero** |
| high (vel 75) | −62.7 dBFS | −42.6 dBFS | **15.4 dB below** | 3.7–15.7 ms | **−240 dBFS = exact zero** |

The exact-zero residual doubles as a regression proof: the reed voice is
bit-identical with and without the click (no channel interaction), and the
click never leaks into the sustain (self-end). CC 7 sweep on the click
channel (vel 127): 127→−21.6, 96→−26.4, 64→−33.5, 32→−45.5, 0→silent
(peak dBFS, no reverb) — CC 7 = 64 is a real −12 dB knob position, the
final level trim is the velocity mapping above.

Plugin-in-loop confirmation (cross-render subtraction is NOT usable there —
the wall-clock-throttled file driver shifts event placement between runs):
with `key_click=high` the T4 onset detector (−55 dB threshold) fires
**2.0 ms earlier** on average across all 80 onsets (+1.89 ms vs plain's
+3.92 ms; T1 high −4.3 ms) — the click crosses the threshold before the
reed. With `key_click=low` the bump is at/below the renderer's placement
jitter (the click is genuinely ~25–30 dB below a vel-100 onset: faint by
design). Per-hop inspection at a single T1 onset shows the high click
elevating the first ~10 ms by up to ~14 dB before the reed swells.

### Determinism + variation effect (T4, 10× repeated note 60 @ vel 100)

Per-note segment peaks (each render's own notes; renderer placement jitter
does not move segment peaks — detA/detB agree to 0.000 dB):

| Metric | variation=off (plain) | variation=on |
|---|---|---|
| 10 repeated-note peaks | −27.16 ×10 (identical) | −27.46…−26.56 (all differ) |
| Δ vs reference | 0.00 (all) | −0.30…+0.60 dB (all nonzero) |
| Adjacent repeat |Δ| | **0.00 dB (all 9 pairs)** | 0.20–1.10 dB (never identical) |
| detA vs detB (same config, 2 runs) | — | **max 0.000 dB** (same seed → same pattern) |

Note 67 repeats (slots 10–19) confirm both effects (Δ −0.50…+0.60, detA/detB
0.000). The raw detA/detB files differ byte-wise (event-to-block placement
of the wall-clock renderer — first diff at the 0.199 s lead-in), but the
velocity PATTERN — what the variation system controls — is bit-stable.
Note: the click jitter consumes PRNG draws, so identical *config* (not
identical PRNG position) is the reproducibility contract.

### Regression: variation=off + key_click=off == Phase 5 sound

- T1 (plugin-in-loop, v3 font): peaks −25.7/−41.6/−21.9/−26.1 dBFS, AM
  2.94–3.47 Hz — identical to the Phase 2/3/5 recorded values (within the
  renderer's ±6 ms event jitter for onset/release readings).
- T4: 80/80 onsets, plain repeated-note peaks exactly identical (above).
- **v2 font + key_click=low**: bit-exact no-op vs the plain render (the
  `click_preset_ok_` guard — without it the failed preset selection left a
  quiet duplicate reed voice: +0.7…+3.6 dB bumps; guarded, peaks identical).

### Phrase-level cleanliness with the click on

- **T10 (duplicate NoteOn) + key_click=low**: exactly 2 auto-segments
  (0.70–4.74, 6.21–9.73 s) — NO onset at the duplicate instants (2.5/7.5 s):
  a swallowed duplicate makes no click (no pallet moved), and each note has
  exactly one release tail.
- **T2 legato + key_click=low**: 4 clean phrases, onsets/releases/AM match
  the plain render within measurement resolution.
- **T12 (drone=48,55) + key_click=low**: drone 48/55 fundamentals
  −59.5/−60.3 dBFS (phrase 1) and −60.1/−60.7 dBFS (drone-only tail) —
  identical to the Phase 5 numbers (the drone starts via config, which never
  triggers clicks); melody 69 fundamental −45.9 dBFS, bit-stable; drone-only
  tail envelope flat (−50.7…−44.9 dBFS, no click bursts).

### Config tests / build / smoke

- `run_config_tests.sh`: **267/267** checks (was 213; +54 for the
  `key_click`/`variation` keys — defaults, valid set/get, strict rejection
  (junk/case/trailing space/empty), mid-phrase toggles interleaved between
  note events, duplicate-NoteOn swallow with click on, CC 123 with click on,
  pre-init storage + apply + live changes).
- Clean build (rm -rf build): **0 warnings** (-Wall -Wextra -Wpedantic).
- Live CLI smoke (timeout 5, `--midi 14:0` Midi Through — the Q49 was not
  connected): exit 124 (alive), startup shows the v3 font path and
  `Synth click: key_click=off variation=on (ch12 preset 2 CC7=64, vel
  low/high=45/75, jitter main +-1..3 click +-1..8, seed 20260919)`, no
  errors.

Renders: `tests/renders/p6_*.wav` (gitignored). Calibration MIDIs/WAVs in
/tmp/opencode/p6 (click_selfend, reedonly/both_low/both_high, cc7_sweep).

## Coupler acoustic check — subsonic-tuning bug fix (2026-09-19)

**The bug.** With the coupler ON, only the main note was audible — the
octave-up voice (note+12, ch 15) never sounded, in the user's live playing
AND in plugin-in-loop renders, despite `status` reporting "on". Root cause
(`plugins/harmonium/harmonium_plugin.cpp`, `apply_coupler_detune`):
`fluid_synth_activate_key_tuning` was fed a 128-entry array of constant
`kCouplerDetuneCents` (+3.0) — but that API expects each key's **ABSOLUTE
pitch in cents** (equal-temperament default = 100·key). Every key was thus
"tuned" to 3 cents ≈ 8 Hz: every coupler voice played as an inaudible
subsonic rumble (a ~17 Hz pulse-train artifact of the extreme down-pitch
resampling), consuming polyphony and adding power, but producing no octave.

**Render A/B proof (T14 probe: notes 48/60/72 held 3 s each, plugin-in-loop,
broken build).** Mid-sustain windows, coupler=off vs coupler=on (pre-init):

- Everything above 260 Hz **bit-identical between renders to ±0.1 dB** —
  no octave line was added anywhere.
- All the ON render's extra power (+0.66 dB total) sat **below 260 Hz**
  (bands: 0–40 Hz +36.7 dB over digital silence, 40–150 Hz +50.6 dB,
  150–260 Hz +41.4 dB) — a 34.4/51.7/68.9/86.1/103.3/137.8 Hz line family
  (~17.2 Hz pulse-train harmonics), i.e. the mistuned voice's rumble.

**Raw-FluidSynth bisect** (scratch harness replicating the plugin's exact
init on ch 15, note 72): (A) plugin's sequence incl. the broken tuning →
the garbage pulse train, nothing at 555 Hz; (B) same minus the tuning calls
→ clean 555.0 Hz voice at the correct CC7=60 level (−8.8 dB vs ch 0
control); (G) pitch array fixed to `100·key + 3` → clean voice at
**556.1 Hz = 555.0 × 2^(3/1200)** — the +3¢ detune audibly applied. All
`apply` flag combinations (0/1) of both tuning calls produced the same
broken result — the pitch array itself was the sole culprit.

**The fix + measured result.** `pitch[key] = 100.0 * key +
kCouplerDetuneCents`. Post-fix renders (same T14 A/B):

| Metric (note 60 window) | broken | fixed |
|---|---|---|
| sustain power ON vs OFF | +0.57 dB | +0.50 dB |
| added power below 1.4·f0 (per-bin clipped) | 99.4% | **0.01%** |
| coupler 2nd-partial zone (≈4·f0, 1093–1135 Hz) bins growing ≥6 dB | 0 (max +1.2 dB) | **20–21 (max +47 dB)** |
| layer level vs main (power subtraction) | n/a (garbage) | **≈ −9.1 dB** |

The fixed coupler's line clusters appear at 1110–1113 Hz and 1665–1669 Hz
(2nd/3rd partials, +3¢ above the main's even partials); its 555 Hz
fundamental hides inside the main's 2nd-partial beating cluster
(coherent ±interference — which is why the permanent check asserts the
2nd-partial zone, not the fundamental).

**Keyboard sweep (T15, notes 24–108 step 6, fixed build).** The octave
voice is present across the whole range: added power is 97.5–100% above
1.4·f0 for every swept note (power growth +0.36…+0.57 dB). No font-zone
holes: note+12 ≤ 120 stays inside the font's stretched top zone; the
range clamp only bites above note 115.

**Correction to the Phase 4 record.** The Phase 4 coupler numbers below
("−6.4…−7.8 dB layer level", "coupler fundamental at 555.0 Hz", the T3/T5
"coupler lines") were measured while this bug was live: the power
subtraction captured the rumble's power, and the "coupler lines" were the
main voice's own beating sidebands (they are present in coupler-off renders
too). The **sub-octave (ch 14) Phase 4 numbers remain valid** — that
channel never had a tuning. The corrected coupler level is ≈ −9 dB below
the main voice at CC7=60.

**Permanent gate.** `tests/scripts/check_coupler_acoustic.sh` +
`coupler_acoustic_assert.py` (probe `tests/midi/T16_coupler_acoustic.mid`,
committed; regenerable via `gen_probe_coupler_acoustic.py`) render the
OFF/ON pair through the real plugin and assert: presence (+0.15…+1.5 dB),
octave-band placement (≥80% of added power above 1.4·f0), and ≥3 bins
growing ≥6 dB in the ≈4·f0 zone. Verified to PASS on the fixed build
(presence +0.50 dB, 99.99% high, 20 bins ≥6 dB, max +47 dB) and **FAIL on
the broken build** (99.4% low, 0 bins) — a reintroduced silent/wrong-pitch
coupler can no longer pass "status says on" unnoticed. Wired into
`tests/e2e_cli_coupler.sh` as Check 4.

**Semantics alignment (user spec).** `set_config("coupler", …)` no longer
retro-applies to held notes: a mid-hold toggle affects NEW presses only;
held notes keep every voice started at their press until their NoteOff
(`HeldNote.layers` bookkeeping). `sub_octave` keeps the Phase 4 retro
semantics (out of scope). Config tests updated; 287/287 green.

## Coupler parity alignment — same level as main, exactly +12 semitones (2026-09-20)

**The complaint.** Even after the subsonic-tuning fix the user reported the
coupler "doesn't work": pressing Sa did not read as Sa + Sa'. Render
measurements confirmed the octave WAS present — but at **≈ −9 dB below the
main voice** (`kCouplerCC7 = 60` on ch 15, plus a +3¢ detune shifting it
off the exact octave). Two spec violations: (1) a separate level curve
(the spec: "the original note and coupled note must receive the same
velocity. Do not introduce a separate velocity curve for the Coupler"),
(2) not exactly 12 semitones (the spec: "the octave difference is exactly
12 MIDI semitones").

**The change** (`plugins/harmonium/` only):
- `kCouplerCC7` 60 → **100** — FluidSynth's default channel volume, the
  same value the untouched main channels sit at. Combined with the note's
  own `played_velocity`, the coupler voice is constructed IDENTICALLY to
  the main voice of note+12: same preset, same channel gain, same
  velocity. No separate level curve, parity by construction.
- The +3¢ detune was REMOVED entirely: `apply_coupler_detune()`,
  `kCouplerDetuneCents` and all MIDI-tuning calls deleted — the octave is
  exactly +12 semitones, and with no tuning code left, the 2026-09-19
  absolute-cents bug class is structurally impossible. (The 100·key+3 fix
  from that day is thereby moot and was removed cleanly.)
- Everything else unchanged: same played velocity for both voices,
  per-press `HeldNote.layers` bookkeeping, new-presses-only toggle
  semantics, N+12≤127 clamp, CC11/pitch-bend mirroring, ch 15 reservation.

**Why CC 7 = 100 gives parity (measured).** Probe `tests/midi/T17_coupler_parity.mid`
(notes 48/60/72/84 held 3 s each; generator
`tests/scripts/gen_probe_coupler_parity.py`), rendered OFF/ON through the
real plugin; measurements by `tests/scripts/coupler_parity_measure.py`:

| Measurement | old build (CC7=60, +3¢) | parity build (CC7=100, exact) |
|---|---|---|
| added octave voice vs main, note 48 | −16.4 dB | **−0.22 dB** |
| added octave voice vs main, note 60 | −9.11 dB | **−0.39 dB** |
| added octave voice vs main, note 72 | −9.50 dB | **−1.19 dB** |
| added octave voice vs main, note 84 (not gated) | −9.16 dB | −2.74 dB |

(added-voice level = implied by the ON/OFF mid-sustain power growth:
`10·log10(10^(presence/10) − 1)`; exact-octave spectral overlap makes
coherent cross terms bias this by up to ~1 dB — the pair-line measurement
below is the unbiased one.)

The **pair-line measurement** (OFF render only, interference-free): the
consecutive probe notes are octave pairs, and at CC7=100 the coupled
octave of N is constructed identically to the main voice of N+12 — so the
fundamental-line delta between them is the clean octave-vs-main level:

| Pair (main → octave voice) | f(main) → f(octave) | line delta |
|---|---|---|
| 48 → 60 | 138.5 → 277.0 Hz | **−0.16 dB** |
| 60 → 72 | 277.0 → 555.0 Hz | **−0.47 dB** |
| 72 → 84 | 555.0 → 1111.0 Hz | **−1.30 dB** |

Note 84's octave (note 96) sits at −2.7 dB: the font's top zone is a
single F4 sample stretched up to +19 semitones (documented Phase 0
finding) — pressing key 96 itself sounds equally thin; parity of
construction holds regardless. The gate therefore asserts the
representative notes 48/60/72.

**T16 audibility gate re-derived** (`coupler_acoustic_assert.py`,
note 60 OFF/ON renders, onset-relative windows):

| Metric | old thresholds (CC7=60, +3¢) | old build | parity thresholds | parity build |
|---|---|---|---|---|
| sustain presence (ON vs OFF) | +0.15…+1.5 dB | +0.50 dB | **+1.5…+4.5 dB** | **+2.82 dB** |
| added power below 1.4·f0 | ≤20% | 0.01% | ≤20% (unchanged) | 0.00% |
| ≈4·f0 zone bins growing ≥6 dB | ≥3 | 20–21 (max +47) | ≥3 (unchanged) | **28 (max +51.8)** |

Two equal-power voices give +3.0 dB presence; the +1.5 dB floor fails the
old too-subtle CC7=60 build (+0.50 dB) and the subsonic-rumble build
(+0.57 dB), the +4.5 dB ceiling fails a runaway layer. With the detune
gone, the coupler's ≈4·f0 2nd partial lands on the main's weak 4th
harmonic and the zone grows even more strongly than before (the old
"+3¢ sideband cluster" trick is no longer needed). Verified FAIL cases:
identical OFF/ON renders (presence +0.00, 0 bins) and the old CC7=60
renders (presence +0.11 < 1.5, 54% of added power mis-placed low at note
48 — its detuned octave destructively interfered with the main's 2nd
harmonic, the −16 dB outlier above).

**Permanent gates.** `check_coupler_acoustic.sh` now renders BOTH probes:
T16 → `coupler_acoustic_assert.py` (audibility: parity presence band,
octave-band placement, ≈4·f0 line growth) and T17 →
`coupler_parity_measure.py --gate` (per-note parity: added voice within
±2 dB of main at 48/60/72 + pair lines within ±2 dB). Wired into
`tests/e2e_cli_coupler.sh` as Check 4.

**Full-stack e2e (new Check 5).** The entire user chain in one pass:
stdin `coupler on` → PluginManager → `set_config`, plus REAL ALSA
NoteOn/NoteOff from a virtual source client (`tests/scripts/midi_poke.cpp`
— a sequencer client with a READ/SUBS_READ port like a hardware keyboard;
built ad hoc into the harness scratch dir) → CLI `--midi` subscription →
PluginManager → plugin layer router → FluidSynth "file" render →
`coupler_acoustic_assert.py` on the CLI-produced OFF/ON WAVs (onset-
relative windows; the note lands at wall-clock-dependent positions).
Measured on the parity build: **presence +2.82 dB — identical to the
plugin-in-loop render**, i.e. nothing in the CLI/ALSA/routing chain
attenuates the octave. Subscription-race note (proven during development):
a note sent before the CLI's subscription is established renders as
digital silence — the harness synchronizes on the CLI's "Listening for
MIDI" line via the helper's `waitfile` command before injecting.

**Config tests / build.** `run_config_tests.sh`: **287/287** (no seam-level
assertions touched coupler level/detune — CC 7 and tuning are FluidSynth
channel state, invisible at the config seam; the acoustic gates own
them). Clean rebuild: **0 warnings**. `tests/e2e_cli_coupler.sh`: **ALL
5 CHECKS PASSED**.

**What the user hears now.** With `coupler on`, every new press sounds as
the note plus its octave at the SAME loudness (within ~1 dB), exactly 12
semitones up — no slow main-vs-octave beat (the +3¢ shimmer is gone by
spec), a full octave-doubling harmonium coupler sound.

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
  **CORRECTION (2026-09-19, same day):** the coupler figures in this bullet
  were measured while the subsonic-tuning bug was live — they captured the
  mistuned voice's rumble power and the main voice's own beating sidebands,
  not an octave voice (the sub-octave figures stand). See "Coupler acoustic
  check" above for the corrected, line-verified coupler at ≈ −9 dB.
- Phase 5 (2026-09-19): the drone (ch13 fixture, CC7=45, fixed vel 100)
  measures ≈11–14 dB under the melody fundamental line with melody lines
  bit-identical on/off (no bellows/gain interaction); the double stop
  applies to the drone channel too (0.50 Hz resolved beat on drone note
  55); CC 123 takes the drone to the s16 floor along with everything else.
- Phase 6 (2026-09-19): the click preset (ch12, CC7=64, vel 45/75) measures
  24.4/15.4 dB below the reed onset peak (faint tick / audible tick) and
  self-ends ≤21 ms after onset (verified: 4 s hold = digital silence;
  click-render minus reed-only residual = exact zero after 60 ms — the reed
  voice is bit-identical with/without the click). variation=off renders
  exactly like Phase 5 (repeated-note peaks identical to 0.00 dB);
  variation=on (fixed-seed mt19937, ±1..3 main / ±4..8 click, anti-repeat)
  makes every note differ (−0.30…+0.60 dB, adjacent repeats never
  identical) with a bit-stable pattern across runs (0.000 dB). Renderer
  caveat: the plugin-in-loop file driver is wall-clock throttled — same
  config does NOT give byte-identical WAVs (event-to-block placement
  jitter); per-note segment peaks are the robust determinism metric, and
  the fluidsynth CLI renderer IS byte-deterministic (used for all exact
  click measurements).
