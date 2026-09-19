# Piano SoundFont

## Current font: GeneralUser GS 1.44 (`GeneralUserGS.sf2`, ~30 MB)

- **Author**: Samuel Christian Collins
- **License**: Royalty-free — free for personal and commercial use
  (see the license text at <http://www.schristiancollins.com/generaluser.php>)
- **Format**: SoundFont 2 (RIFF), General MIDI bank — preset 0 is an acoustic
  grand piano; 128 GM instruments accessible via MIDI program change
- **Committed directly to git** (under GitHub's 100 MB limit; no LFS needed)

## Why the previous font (SalamanderGrandLite.sf2) was replaced

The Salamander Grand Piano Lite SF2 (a third-party Polyphone conversion of the
Salamander Grand Piano V3 SFZ, CC BY 3.0, from
VimHater/SalamanderGrandLite_sf2) had a **broadband click impulse baked into
the onset of every note sample** — diagnosed by offline rendering
(`fluidsynth -F out.wav font.sf2 test.mid`) and sox analysis:

- Maximum sample-to-sample delta in the 10–20 ms window after note-on:
  **0.0399** (Salamander) vs **0.0053** (GeneralUser) — ~45x sharper
- Spectrogram: a bright vertical line (full-spectrum impulse) at every note
  onset, absent in clean fonts

Because the discontinuity sits ~10–20 ms *into* the sample (not at sample
start), no volume-envelope attack shaping (`GEN_VOLENVATTACK`) can remove it.
The file was defective; the font was swapped. The original Salamander SFZ
samples remain the reference if a clean full conversion is ever produced
(<https://github.com/sfzinstruments/SalamanderGrandPiano>, CC BY 3.0).

## Attack shaping

The plugin exposes an `attack_ms` config key (default **1** = SF2 default =
no change). Raise it (e.g. 10) to soften the note onset if desired; the
mechanism mirrors the harmonium plugin's additive `GEN_VOLENVATTACK` offset.
