# Harmonium SoundFonts (plugin-local provenance)

All SoundFonts the harmonium plugin can use live in this directory — the
plugin is **self-contained**: no font outside this repository is needed to
build, run, or re-derive anything. Fonts are committed as blobs
(~6.6 MB each) because they are the reproducible provenance of the
plugin's sound.

| File | Role | Notes |
|---|---|---|
| `harmonium_original.sf2` | **Upstream source** (the only sample-data origin) | 1 preset ("harmonium"), 1 instrument, 14 reed samples. Original harmonium recording by rtalwar26, 2018, **ISC license** (the midi-harmonium project; recorded in the font's INFO chunk — see `docs/HARMONIUM_SF2_AUDIT.md`). Never modified; input to every derivation. |
| `harmonium_v2.sf2` | Derived font (Phase 3, kept for comparability) | = original + preset 1 "harmonium double" (every key zone duplicated with `fineTune = +4¢`). Build default before Phase 6. |
| `harmonium_v3.sf2` | Derived font (Phase 6) — **current CMake default** | = v2 + preset 2 "key click" (self-ending synthesized chiff sample + zone). Preset 0 renders sample-identically to the original. |

## Checksum (provenance anchor)

`harmonium_original.sf2` is a byte-identical copy of the original upstream
font (verified with `cmp`):

```
156945f812767f0df8b8b82558003c71601a2ee6942833d5239d32601d5bfeff  harmonium_original.sf2
```

## Derivation chain

```
harmonium_original.sf2
        │  tests/scripts/derive_sf2.py            (default input)
        ▼
harmonium_v2.sf2        (+4¢ duplicated zones, preset 1)
        │  tests/scripts/derive_sf2.py --click    (default input)
        ▼
harmonium_v3.sf2        (+ synthesized 40 ms key-click sample, preset 2)
```

The derivation is deterministic: re-running `derive_sf2.py` from
`harmonium_original.sf2` reproduces the committed v2/v3 byte-identically
(verified 2026-09-19 — the script only rewrites structure chunks; the
sample data is referenced, not regenerated).

Re-derivation commands (from the repo root):

```bash
python3 tests/scripts/derive_sf2.py \
    plugins/harmonium/soundfonts/harmonium_original.sf2 \
    plugins/harmonium/soundfonts/harmonium_v2.sf2 4 "harmonium double"
python3 tests/scripts/derive_sf2.py \
    plugins/harmonium/soundfonts/harmonium_original.sf2 \
    plugins/harmonium/soundfonts/harmonium_v3.sf2 --click
```

(The input argument is optional — omit it to get the same default.)

## Audit

Structural details (zones, generators, envelope bases, known limitations
such as the stretched top octave) are documented in
`docs/HARMONIUM_SF2_AUDIT.md`.
