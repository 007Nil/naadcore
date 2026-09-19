# NaadCore Implementation Summary

Last updated: 2026-09-19 (harmonium realism effort Phases 0–6 COMPLETE —
harmonium plugin feature-complete; next NaadCore focus is other plugins)

## Overview

NaadCore is a Linux-native harmonium synthesizer built on a plugin architecture.
The system is **complete and validated end-to-end**: a Q49 MIDI keyboard
(ALSA 20:0) drives `naadcore-cli`, which loads the `harmonium` plugin
(`libharmonium_plugin.so`) through the `PluginManager`; the plugin runs an embedded
FluidSynth instance with the compiled-in harmonium SoundFont and outputs through ALSA.

```
Q49 → ALSA sequencer → naadcore-cli → PluginManager → harmonium plugin
    → embedded FluidSynth + harmonium.sf2 → ALSA audio → speakers
```

## Status

- ✅ **Harmonium realism effort COMPLETE (Phases 0–6, closed 2026-09-19)** —
  the harmonium plugin is feature-complete per the realism plan:
  - Phase 0: test harness (`tests/` — T1–T13 MIDI tracks, offline +
    plugin-in-loop renderers, live capture, analyze.py, config tests)
  - Phase 1: pinned synth voicing (gain 0.4, small-room reverb, chorus
    off, 4th-order interpolation) + live config keys
  - Phase 2: runtime envelope shaping (attack_ms=10, release_ms=200 via
    additive FluidSynth channel generators, empirically calibrated)
  - Phase 3: derived in-repo SoundFont (`harmonium_v3.sf2` presets:
    single, double-reed +4¢, key-click) + `stop` key; PROGRAM_CHANGE
    events ignored (stops are config-controlled)
  - Phase 4: layer router — octave coupler (ch15, note+12, −6..−8 dB,
    +3¢) + sub-octave (ch14, note−12) with mid-phrase toggling; duplicate
    NoteOn ignored; CC 123 hardened to all channels; cross-channel offs
  - Phase 5: drone (ch13 fixture notes, `drone`/`drone_level`) +
    authoritative config-key registry (`docs/HARMONIUM_CONFIG.md`)
  - Phase 6: key-click layer (ch12, self-ending instrument) +
    deterministic per-note micro-variation (`variation`)
- ✅ **Verification**: 267/267 config-seam tests, objective measurements in
  `tests/RESULTS.md` (onset/release/AM/beat-rate/level numbers per phase),
  reference A/B listening package prepared (yt-dlp + sox installed),
  zero-warning builds throughout
- ✅ **Uniform bellows velocity implemented** — chords sound at the first key's
  velocity; when the reference key is released mid-chord, the reference passes
  to the oldest still-held key's original press velocity (plugin-local, in
  `HarmoniumPlugin::handle_midi_event`; see `HANDOVER.md`).
- ✅ **MVP app removed** — the original `naadcore-harmonium` executable and its
  sources (`apps/harmonium.cpp`, `core/harmonium.hpp`,
  `include/naadcore/harmonium.hpp`) were deleted; the plugin workflow fully
  supersedes them.
- ✅ **Duplicate headers cleaned** — `core/midi.hpp` (duplicate of
  `include/naadcore/midi.hpp`) and `core/plugin_manager.hpp` (duplicate of
  `include/naadcore/plugin_manager.hpp`, removed earlier) are gone. All headers now
  live canonically under `include/naadcore/` and all sources include them via the
  `"naadcore/..."` prefix.
- ✅ Exactly 3 build targets remain: `naadcore_core`, `naadcore-cli`,
  `harmonium_plugin`.

## Key features

- **Plugin interface** (`include/naadcore/plugin.hpp`): `INaadPlugin`,
  `MidiEvent`, `PluginInfo`, `PluginResult`, plus required C-linkage exports
  `naad_plugin_create()`, `naad_plugin_destroy()`, `naad_plugin_get_version()`.
- **Plugin manager** (`core/plugin_manager.cpp`, `include/naadcore/plugin_manager.hpp`):
  singleton with `dlopen(RTLD_NOW | RTLD_LOCAL)`/`dlsym` loading, mutex-protected
  lifecycle, and MIDI event fan-out routing.
- **MIDI input** (`core/midi.cpp`, `include/naadcore/midi.hpp`): ALSA sequencer
  client registered as `naadcore`, non-blocking event drain, callback dispatch.
- **Harmonium plugin** (`plugins/harmonium/`): embedded FluidSynth, derived
  in-repo SoundFont (`soundfonts/harmonium_v3.sf2`; presets single/double-reed/
  key-click), full MIDI event handling, uniform bellows velocity, and the
  realism feature set — pinned voicing, runtime envelope shaping, reed stops,
  octave coupler + sub-octave layers, drone, key-click layer, per-note
  micro-variation; 14 config keys documented in `docs/HARMONIUM_CONFIG.md`.
- **CLI** (`apps/naadcore-cli/`): `--plugin <path>` (required), `--midi <client:port>`,
  `--audio-driver <name>` (wired through to plugins), `--help`.
- **Launcher** (`naadcore.sh`): interactive wizard — build, MIDI scan/menu,
  plugin menu, driver menu, run.

## Fixed critical bugs (historical, from MVP/plugin bring-up)

1. **MIDI event delivery**: port capabilities changed from
   `SND_SEQ_PORT_CAP_READ|SUBS_READ` to `SND_SEQ_PORT_CAP_WRITE|SUBS_WRITE` —
   the root cause of events never reaching the application.
2. **Audio driver selection**: explicit `audio.driver` = "alsa" instead of
   auto-selection, which could fail on an uninitialized SDL3 build (silence).
3. **Event processing**: drain all pending events per poll instead of one.
4. **Port subscription**: use the parsed port number, not hardcoded port 0.

## Build and run

```bash
# Build (from project root)
cmake -B build && cmake --build build -j4

# Run (CLI runs forever; use timeout or Ctrl+C)
timeout 5 ./build/apps/naadcore-cli/naadcore-cli \
    --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0
```

Outputs: `build/libnaadcore_core.so`, `build/apps/naadcore-cli/naadcore-cli`,
`build/plugins/libharmonium_plugin.so`.

## Verification

The implementation was verified through:
1. Clean rebuild with exactly 3 targets, zero errors.
2. CLI smoke run: SoundFont load, plugin load ("harmonium v1.0.0"), ALSA audio
   driver start, single "MIDI input connected: 20:0 -> ..." line, readiness message.
3. MIDI end-to-end: `aseqsend -p <port> "90 60 100"` / `"80 60 0"` against the
   running CLI; process stays alive, no errors in log.
4. Uniform bellows velocity: instrumented `aseqsend` matrix covering chord
   inheritance, baton-pass on reference-key release, fresh sequences after
   full release, vel-0 NoteOn, duplicate NoteOn, phantom NoteOff, and CC 123
   reset — all pass.
5. Real-world key-press testing on the Q49 (velocity sensitivity + polyphony).
6. Realism phases: config-seam unit tests (267/267 via
   `tests/scripts/run_config_tests.sh`), plugin-in-loop offline renders +
   objective WAV analysis per phase (onset/release/AM/beat rate/levels,
   `tests/RESULTS.md`), live capture through the full CLI chain, and a
   loudness-matched reference A/B listening package.

## Project structure

```
naadcore/
├── CMakeLists.txt                  # Root build (3 targets)
├── naadcore.sh                     # Interactive launcher wizard
├── include/naadcore/               # Canonical headers: plugin.hpp, plugin_manager.hpp, midi.hpp
├── core/                           # Library sources: midi.cpp, plugin_manager.cpp
├── apps/naadcore-cli/              # CLI application (plugin loader)
├── plugins/harmonium/              # Harmonium plugin (self-contained; committed
│   │                               #   fonts in soundfonts/: original + v2 + v3
│   │                               #   default — see soundfonts/README.md)
├── tests/                          # Realism harness (tracks, renderers, analysis, tests)
└── docs/                           # Architecture + plugin + config + audit documentation
```

See `HANDOVER.md` for the authoritative, detailed current-state document.