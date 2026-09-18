# Harmonium SoundFont Audit (Phase 1)

Audit of `/home/nil/harmonium-companion/harmonium.sf2` (the SoundFont
compiled into the harmonium plugin via `HARMONIUM_SOUNDFONT_PATH`).

Method: direct binary parse of the RIFF/sfbk container with
`tests/scripts/sf2_audit.py` (pure Python, parses INFO/sdta/pdta chunks:
phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr), cross-checked with
the FluidSynth shell (`load` + `inst 2` → `000-000 harmonium`).

## File overview

| Property | Value |
|---|---|
| Size / format | 6.62 MB, SF2 (uncompressed PCM), RIFF/sfbk |
| INAM / IENG | "Harmonium" / "Bytes & Bellows" |
| ICRD / ISFT | 8 June 2026 / Polyphone |
| Provenance (ICMT) | Part of the Harmonium Companion app (MIT); original harmonium recording by rtalwar26, 2018, ISC license (midi-harmonium) |
| Presets | 1: bank 0, program 0, "harmonium" |
| Instruments | 1: "harmonium" (14 zones + 1 global zone) |
| Samples | 14 mono 16-bit, 22050 Hz, 5.1–12.0 s each (~3.3 M frames) |
| Modulators | **none** (pmod/imod contain only terminal records) |

The preset has two zones: an empty global zone and one zone referencing
instrument 0 over the full key/velocity range.

## Key mapping (instrument zones)

One sample per natural note around octaves 2–4, black keys borrow the
nearest lower natural sample; extremes stretch a single sample:

| Keys (MIDI) | Sample | Root | Stretch at zone edges |
|---|---|---|---|
| 0–43 | G2 | 43 | down to −43 (harmonium range 36–43 → −7) |
| 44–45 | A2 | 45 | −1 |
| 46–47 | B2 | 47 | −1 |
| 48 | C3 | 48 | 0 |
| 49–50 | D3 | 50 | −1 (C#3) |
| 51–52 | E3 | 52 | −1 (D#3) |
| 53 | F3 | 53 | 0 |
| 54–55 | G3 | 55 | −1 (F#3) |
| 56–57 | A3 | 57 | −1 (G#3) |
| 58–59 | B3 | 59 | −1 (A#3) |
| 60 | C4 | 60 | 0 |
| 61–62 | D4 | 62 | −1 (C#4) |
| 63–64 | E4 | 64 | −1 (D#4) |
| 65–127 | F4 | 65 | up to +62 (harmonium range 65–84 → **+19 max**) |

**Does the SF2 cover notes 36–84?** Structurally yes — every key in 36–84
maps to a zone — but not with even timbre:

- The entire top octave (65–84, upper Sa and above) is the single F4
  sample transposed up as much as +19 semitones — expect an increasingly
  "sped-up" reed character high up.
- Keys 36–43 use the G2 sample transposed down up to −7 semitones —
  duller/darker than the recorded pitch.
- Around C3–E4 (48–64) mapping is near 1:1 — this is the font's sweet
  spot; measured onsets (23–58 ms) and uniform timbre in T1/T2 confirms
  this region is solid.

## Volume envelope (global instrument zone)

| Generator | Value | Meaning |
|---|---|---|
| attackVolEnv | default (−12000) | ≈ 1 ms — **instant attack**; all "reed speech" is the recorded transient in each sample |
| holdVolEnv | default | ≈ 1 ms |
| decayVolEnv | −5186 (50 ms) | moot: sustainVolEnv defaults to 0 (full sustain, infinite) |
| sustainVolEnv | default 0 | no decay while held (organ-like reed behavior — correct for harmonium) |
| releaseVolEnv | −3986 (**100 ms**) | the only explicit envelope setting; measured T1 release ≈ 50 ms to the noise floor (FluidSynth's curve reaches −60 dB before the nominal full decay) |
| sampleModes | 1 | loop continuously during sustain |

## Loops

All 14 samples loop (sampleModes=1), loop lengths 2.0–9.8 s (median ~5.5 s).
The loops retain the recorded reed beating: T1 measurements show sustained
amplitude modulation of ~3 Hz with 2.5–6 dB depth — this is natural
harmonium shimmer baked into the samples (good), though loop-seam artifacts
remain possible on long drones (T5 listens for this).

## Velocity, filters, LFO, effects — all absent

- **Velocity layers: none.** Every zone spans vel 0–127.
- **Velocity modulators: none.** No velocity→filter, no velocity→attack.
  Velocity changes loudness only (via the default velocity→attenuation
  modulator). The harmonium-companion app had the same behavior — dynamics
  were meant to come from the bellows expression (CC#11), not velocity.
- **LFO vibrato: none.** No LFO generators set (freqVibLFO, vibLfoToPitch,
  modLfoToVolume all default 0). All motion is in the samples.
- **Filter: default.** initialFilterFc defaults to 13500 Hz (effectively
  open), Q 0 — the reeds' spectral character is entirely from the samples.
- **Pan / reverb / chorus sends: default.** No per-zone pan;
  reverbEffectsSend defaults to 20% (200, SF2 spec), chorus 0.

## Sample rate note

Samples are 22050 Hz → spectral content capped at ~11 kHz. The plugin
resynthesizes at 44100 Hz; 4th-order interpolation (Phase 1 default) is
the right call — 7th-order sinc would add no real content here.

## Answers to the audit's key questions

1. **Key coverage 36–84?** Yes structurally; timbre uneven above 65
   (single F4 sample stretched up to +19 st) and below 44 (G2 stretched
   down to −7 st).
2. **Velocity→filter/attack modulators?** No. None exist.
3. **LFO vibrato?** No. Sustain motion (~3 Hz, 2.5–6 dB) is recorded into
   the looped samples.
4. **Attack/release values?** Attack ≈ 1 ms (default, instant), release
   100 ms (explicit), sustain infinite (organ-like — correct).

## Implications for the realism plan (Phase 2+ candidates)

1. **Release tail**: 100 ms release is short for a reed's after-ring;
   consider lengthening releaseVolEnv (e.g. 250–400 ms) via live config
   or per-preset default once measured against a reference.
2. **Top-octave stretch**: the 65–84 region will never sound like real
   high reeds from one stretched sample — mitigation options: pitch
   limitation (clamp), a second high sample set, or accept and document.
3. **No velocity timbre**: since a real harmonium's dynamics come from
   bellows pressure, not key speed, this is *physically fine* — but it
   means perceived dynamics will rely entirely on the (already
   uniform-bellows) velocity model, and velocity sweeps (T7) will show
   amplitude-only change.
4. **Loop seams on long drones**: verify T5 by listening; if seam clicks
   appear, note loop points for a Polyphone fix.

## Not verifiable without GUI tools (needs Polyphone)

- Loop-seam continuity (waveform-level click inspection)
- Sample start/end trimming quality and DC offset
- Visual check of the attack transient shape per sample
- Any subjective sample-quality assessment (recording noise floor etc.)

These are recorded as follow-ups; the binary-level facts above are
complete and machine-verified.
