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
