# NaadCore Implementation Summary

Last updated: 2026-09-17 (post-cleanup state)

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

- ✅ **Plugin system complete and validated** — CLI loads the plugin, audio starts,
  MIDI events route to the synth, notes play.
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
- **Harmonium plugin** (`plugins/harmonium/`): embedded FluidSynth, compile-time
  SoundFont path, full MIDI event handling (Note On/Off, CC, Pitch Bend,
  Program Change, Channel/Key Pressure).
- **CLI** (`apps/naadcore-cli/`): `--plugin <path>` (required), `--midi <client:port>`,
  `--audio-driver <name>`, `--help`.

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
4. Real-world key-press testing on the Q49 (velocity sensitivity + polyphony).

## Project structure

```
naadcore/
├── CMakeLists.txt                  # Root build (3 targets)
├── include/naadcore/               # Canonical headers: plugin.hpp, plugin_manager.hpp, midi.hpp
├── core/                           # Library sources: midi.cpp, plugin_manager.cpp
├── apps/naadcore-cli/              # CLI application (plugin loader)
├── plugins/harmonium/              # Harmonium plugin (embedded SoundFont)
└── docs/                           # Architecture + plugin documentation
```

See `HANDOVER.md` for the authoritative, detailed current-state document.