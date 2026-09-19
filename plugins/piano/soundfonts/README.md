# Salamander Grand Piano Lite SoundFont

## Source

**SalamanderGrandLite.sf2** — a SoundFont 2 conversion of the *Salamander Grand
Piano V3 Lite* (Yamaha C5, "Slender Edition" 2017).

- **Original format**: SFZ (see https://github.com/sfzinstruments/SalamanderGrandPiano)
- **Converted to SF2** using Polyphone
- **Author**: Alexander Holm <axeldenstore@gmail.com>
- **Phase alignment**: Signal Experiments (sig-ex.com)

## License

Creative Commons Attribution 3.0 Unported (CC BY 3.0) — same as the original
Salamander Grand Piano.

See https://creativecommons.org/licenses/by/3.0/ for the full license text.

## Download

Released at:
https://github.com/VimHater/SalamanderGrandLite_sf2/releases/tag/0.1

## Why SF2?

FluidSynth 2.4.8 on Debian does not include SFZ support (no `libsfz`
dependency). The SF2 format is FluidSynth's native SoundFont container format
and is loaded via `fluid_synth_sfload()` — the same mechanism used by the
harmonium plugin.

## File

| Property | Value |
|---|---|
| Format | SoundFont 2.04 (RIFF-based) |
| Size | ~184 MB |
| Version | Lite (Slender Edition, reduced compared to the full V3) |
| Instrument | Grand piano (Yamaha C5) |
