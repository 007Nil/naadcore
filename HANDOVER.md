# NaadCore Handover — Authoritative State Document

Last updated: 2026-09-17 (after MVP removal + plugin-system validation session)

## Project purpose

NaadCore is a Linux-native harmonium synthesizer built as a modular, plugin-based
framework. A MIDI keyboard (Alesis Q49 on ALSA sequencer port 20:0) drives
dynamically loaded instrument plugins; the first and currently only plugin is the
FluidSynth-based harmonium. Goal: press Q49 keys → hear a velocity-sensitive,
polyphonic harmonium through ALSA audio.

## Current architecture (validated end-to-end)

```
Alesis Q49 (ALSA client 20:0)
        ↓ ALSA sequencer
naadcore-cli (apps/naadcore-cli/main.cpp)
  - opens MidiInput (core/midi.cpp) and subscribes to 20:0
  - converts ALSA events → plugin MidiEvent structs
  - routes every event through PluginManager
        ↓
PluginManager (core/plugin_manager.cpp, header include/naadcore/plugin_manager.hpp)
  - singleton; dlopen(RTLD_NOW | RTLD_LOCAL) + dlsym on plugin .so
  - lifecycle: load → init → start_audio → route_midi_event* → stop/cleanup
        ↓
libharmonium_plugin.so (plugins/harmonium/)
  - implements INaadPlugin
  - owns its own FluidSynth settings/synth/audio-driver
  - SoundFont path compiled in (embedded at build time)
        ↓
FluidSynth + /home/nil/harmonium-companion/harmonium.sf2 → ALSA audio → speakers
```

The CLI registers with ALSA under the client name **`naadcore`** and creates a
write-capable input port ("naadcore input") that subscribes to the Q49 port at
startup — no manual `aconnect` needed.

## Complete post-cleanup file map

```
naadcore/
├── CMakeLists.txt                  # Root build — exactly 3 targets (see below)
├── README.md                       # Build/run instructions (CLI + plugin workflow)
├── HANDOVER.md                     # This file
├── IMPLEMENTATION_SUMMARY.md       # Implementation history/status
├── AGENTS.md                       # Agent usage instructions
├── .gitignore
├── include/naadcore/               # Canonical public headers
│   ├── plugin.hpp                  # INaadPlugin, MidiEvent, PluginInfo, PluginResult, C factory typedefs
│   ├── plugin_manager.hpp          # PluginManager singleton API
│   └── midi.hpp                    # MidiInput + Synthesizer class declarations
├── core/                           # Core library (naadcore_core) sources
│   ├── midi.cpp                    # MidiInput (ALSA sequencer) + Synthesizer (FluidSynth wrapper)
│   └── plugin_manager.cpp          # PluginManager implementation (dlopen/dlsym, routing)
├── apps/naadcore-cli/              # CLI application target: naadcore-cli
│   ├── CMakeLists.txt
│   └── main.cpp                    # Arg parsing (--plugin/--midi/--audio-driver/--help), MIDI routing loop
├── plugins/
│   ├── README.md                   # Plugin directory overview
│   └── harmonium/                   # Plugin target: harmonium_plugin
│       ├── CMakeLists.txt          # Embeds HARMONIUM_SOUNDFONT_PATH, outputs to build/plugins/
│       ├── harmonium_plugin.hpp
│       └── harmonium_plugin.cpp     # FluidSynth plugin + extern "C" factories
└── docs/
    ├── NAADCORE_ARCHITECTURE.md    # Architecture overview (current)
    ├── PLUGIN_SYSTEM.md            # Plugin system design/usage
    ├── PLUGIN_SYSTEM_IMPLEMENTATION.md # Implementation notes (current)
    ├── PLUGIN_DEVELOPMENT.md       # Guide for writing new plugins
    ├── NAADCORE_MVP_CHALLENGE.md   # HISTORICAL: MVP design record (completed)
    ├── CODEBASE_ANALYSIS.md        # Analysis of the separate harmonium-companion web project
    └── README.md                   # Technical notes / API reference
```

Removed in the last cleanup (do NOT recreate): `apps/harmonium.cpp`,
`core/harmonium.hpp`, `include/naadcore/harmonium.hpp`, `core/midi.hpp`
(duplicate of `include/naadcore/midi.hpp`), `core/plugin_manager.hpp`
(duplicate of `include/naadcore/plugin_manager.hpp`), empty `tests/`,
and the `naadcore-harmonium` MVP executable target.

Note: `Synthesizer` (FluidSynth wrapper class in core/midi.cpp) is retained as
core-library API even though the plugin currently embeds its own synth — it is
linked into `naadcore_core` and available to future plugins.

## Build

```bash
cd /home/nil/Projects/Personal/naadcore
rm -rf build            # clean
cmake -B build
cmake --build build -j4
```

Exactly 3 targets:

| Target | Type | Output |
|---|---|---|
| `naadcore_core` | SHARED lib | `build/libnaadcore_core.so` |
| `harmonium_plugin` | SHARED lib (plugin) | `build/plugins/libharmonium_plugin.so` |
| `naadcore-cli` | executable | `build/apps/naadcore-cli/naadcore-cli` |

All includes use the `"naadcore/..."` form and resolve via `include/`; no
`include_directories(.../core)` remains anywhere.

## Run

```bash
# From the project root. The CLI runs forever — use timeout or Ctrl+C.
timeout 5 ./build/apps/naadcore-cli/naadcore-cli \
    --plugin ./build/plugins/libharmonium_plugin.so \
    --midi 20:0
```

Expected console output (order may vary slightly):

```
Loading plugin: ./build/plugins/libharmonium_plugin.so
Loaded SoundFont: /home/nil/harmonium-companion/harmonium.sf2 (ID: 1)
Loaded plugin: harmonium v1.0.0 (./build/plugins/libharmonium_plugin.so)
Plugin: harmonium v1.0.0
Starting audio...
Audio driver started: alsa
MIDI input connected: 20:0 -> 129:0
Ready. Press Ctrl+C to exit.
Listening for MIDI from 20:0
```

`timeout` exit code 124 = PASS (process was alive and healthy until killed).

### MIDI end-to-end test (no keyboard needed)

```bash
# Background run with log capture
./build/apps/naadcore-cli/naadcore-cli \
    --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0 \
    > /tmp/opencode/naadcore-e2e.log 2>&1 &
CLI_PID=$!
sleep 2
aconnect -l            # find the client named 'naadcore', e.g. 129:0
aseqsend -p 129:0 "90 60 100"   # note on  (this aseqsend build uses positional hex)
aseqsend -p 129:0 "80 60 0"     # note off
sleep 1
kill -0 $CLI_PID && echo "process alive"   # should be alive
kill $CLI_PID
```

Note: this system's `aseqsend` uses **positional hex syntax**
(`aseqsend -p <port> "90 60 100"`), NOT the `-e` flag form.

## Plugin interface summary

Defined in `include/naadcore/plugin.hpp`:

- `struct MidiEvent` — `{type, channel, data1, data2, timestamp}` with enum
  `NOTE_ON, NOTE_OFF, CONTROL_CHANGE, PITCH_BEND, PROGRAM_CHANGE,
  CHANNEL_PRESSURE, KEY_PRESSURE`
- `struct PluginInfo` — `{name, version, author, description}`
- `enum PluginResult` — `PLUGIN_OK(0), PLUGIN_ERROR(-1), PLUGIN_NOT_IMPLEMENTED(-2),
  PLUGIN_INVALID_PARAM(-3)`
- `class INaadPlugin` — pure virtual:
  `get_info()`, `init(const char* audio_driver = nullptr)`, `start_audio()`,
  `stop_audio()`, `handle_midi_event(const MidiEvent&)`, `get_config(const char* key)`,
  `set_config(const char* key, const char* value)`

Every plugin `.so` must export three C-linkage factories (looked up via `dlsym`):

```cpp
extern "C" {
    naadcore::INaadPlugin* naad_plugin_create();
    void naad_plugin_destroy(naadcore::INaadPlugin* plugin);
    int naad_plugin_get_version();   // currently 1
}
```

Lifecycle: `dlopen` → `naad_plugin_create()` → `init(driver)` → `start_audio()` →
`handle_midi_event()` per event → `stop_audio()` → `naad_plugin_destroy()` → `dlclose`.

PluginManager (singleton) API: `load_plugin(path)`, `unload_plugin(path)`,
`route_midi_event(event)`, `get_plugin_info(path)`, `is_plugin_loaded(path)`,
`get_loaded_plugins()`, `initialize(audio_driver)`, `start_all_audio()`,
`stop_all_audio()`, `cleanup()`. Thread-safe (recursive mutex).

## Embedded SoundFont mechanism

The harmonium plugin has no runtime SoundFont flag. The path is baked in at
compile time:

- `plugins/harmonium/CMakeLists.txt` sets the CMake variable
  `HARMONIUM_SOUNDFONT_PATH` (default `/home/nil/harmonium-companion/harmonium.sf2`)
  and passes it as a `target_compile_definitions(... PRIVATE)` preprocessor macro.
- `harmonium_plugin.cpp` has a `#ifndef HARMONIUM_SOUNDFONT_PATH` fallback with the
  same path, then loads it via `fluid_synth_sfload()` during `init()`.
- Override for custom builds: `cmake -B build -DHARMONIUM_SOUNDFONT_PATH=/path/to.sf2`

## Harmless warnings (do not chase these)

- FluidSynth SDL3-related startup messages
- GLib `g_param_spec` CRITICALs emitted by FluidSynth
- `No preset found on channel 9` (GM percussion channel, unused)
- `Failed to set thread to high priority`

## Known minor issues

- The CLI loads exactly one plugin per run (PluginManager supports multiple via
  `route_midi_event` fan-out, but the CLI only takes one `--plugin` argument).
- ALSA client numbers for the CLI (e.g. 129) are dynamic; always read the actual
  number from `aconnect -l` rather than assuming.
- `MidiEvent::timestamp` is never filled in by the CLI conversion layer (stays 0).
- The `Synthesizer` class inside `core/midi.cpp` is currently unused by the plugin
  (plugins embed their own FluidSynth instance); it remains as core-library API.
- No CC/passport handling beyond what FluidSynth does natively; no raga/bellows/
  coupler/GUI features yet.

## Suggested next steps

1. **Housekeeping**: make the initial git commit of the cleaned-up tree (the repo
   currently has zero commits — everything is untracked).
2. **Multiple plugin support in CLI**: accept several `--plugin` flags or a plugin
   directory; route MIDI to all loaded plugins (PluginManager already fans out).
3. **Plugin configuration**: wire `set_config/get_config` to CLI flags or a config
   file (e.g. per-plugin channel assignment).
4. **Raga selection**: implement raga note filtering in the harmonium plugin
   (see docs/CODEBASE_ANALYSIS.md for the harmonium-companion raga/sargam logic
   worth porting).
5. **Bellows/expression modeling**: map a MIDI controller (CC#11 or velocity
   envelope) to harmonium air-pressure expression.
6. **Tests**: add a unit/integration test target (tests/ was removed as empty);
   e.g. plugin export checks, PluginManager load/unload tests, ALSA loopback tests.
7. **Additional plugins**: pipe organ or drone/tanpura plugin using the same
   INaadPlugin contract as a portability proof.
8. **Packaging**: install rules exist (`bin`, `lib/naadcore`, `lib/naadcore/plugins`);
   consider CPack or a proper install layout.