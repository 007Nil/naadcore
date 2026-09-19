# NaadCore Technical Documentation

## Overview

NaadCore is a modular, plugin-based harmonium synthesizer for Linux:
Q49 MIDI input → `naadcore-cli` → PluginManager → harmonium plugin →
embedded FluidSynth + harmonium.sf2 → ALSA audio output.

Start with `README.md` (project root) for build/run instructions and
`HANDOVER.md` for the authoritative current-state document.

## Project Layout

```
naadcore/
├── CMakeLists.txt              # Root build configuration (3 targets)
├── include/naadcore/           # Canonical public headers
│   ├── plugin.hpp              # INaadPlugin, MidiEvent, PluginInfo, PluginResult
│   ├── plugin_manager.hpp      # PluginManager singleton API
│   └── midi.hpp                # MidiInput + Synthesizer class declarations
├── core/                       # Core library sources (naadcore_core)
│   ├── midi.cpp                # MidiInput (ALSA) + Synthesizer (FluidSynth wrapper)
│   └── plugin_manager.cpp      # PluginManager implementation (dlopen/dlsym)
├── apps/naadcore-cli/          # CLI application (plugin loader)
│   ├── CMakeLists.txt
│   └── main.cpp                # CLI entry point, MIDI event routing
├── plugins/harmonium/          # Harmonium plugin (reference INaadPlugin implementation)
│   ├── CMakeLists.txt          # Embeds HARMONIUM_SOUNDFONT_PATH (default: in-repo v3)
│   ├── soundfonts/             # Committed fonts (original provenance + derivations)
│   │                           #   + provenance README — the plugin is self-contained
│   ├── harmonium_plugin.hpp
│   └── harmonium_plugin.cpp
└── docs/                       # This documentation directory
```

## Architecture Components

1. **MidiInput Class** (`include/naadcore/midi.hpp`, `core/midi.cpp`)
   - Manages the ALSA sequencer connection (registered as client `naadcore`)
   - Subscribes to a MIDI source client:port
   - Drains pending events non-blocking and dispatches to a callback

2. **Synthesizer Class** (`include/naadcore/midi.hpp`, `core/midi.cpp`)
   - FluidSynth wrapper: settings, SoundFont loading, note on/off, audio driver
   - Core-library API; the harmonium plugin currently embeds its own synth

3. **PluginManager Class** (`include/naadcore/plugin_manager.hpp`, `core/plugin_manager.cpp`)
   - Singleton; loads plugins via `dlopen(RTLD_NOW | RTLD_LOCAL)`
   - Lifecycle management and MIDI event fan-out routing

4. **INaadPlugin Interface** (`include/naadcore/plugin.hpp`)
   - Plugin contract + required C-linkage exports
   (`naad_plugin_create()`, `naad_plugin_destroy()`, `naad_plugin_get_version()`)

5. **HarmoniumPlugin** (`plugins/harmonium/`)
   - Reference plugin: own FluidSynth instance, derived in-repo SoundFont
     (`soundfonts/harmonium_v3.sf2`: presets single / double-reed / key-click)
   - Uniform bellows velocity: keys pressed together sound at the first key's
     velocity; when that key is released, the reference passes to the oldest
     still-held key's original press velocity (see "Uniform Bellows Velocity"
     in `HANDOVER.md`)
   - Realism feature set (Phases 0–6 complete, 2026-09-19): pinned voicing
     (gain 0.4, small-room reverb, chorus off, 4th-order interpolation),
     runtime envelope shaping (attack_ms/release_ms), reed stops (`stop`),
     octave coupler + sub-octave layers (`coupler`/`sub_octave`), drone
     (`drone`/`drone_level`), key-click layer (`key_click`), per-note
     micro-variation (`variation`); live `gain`/`reverb`/`chorus` keys
   - Config-key registry: `docs/HARMONIUM_CONFIG.md`; SF2 audit:
     `docs/HARMONIUM_SF2_AUDIT.md`; realism verification: `tests/RESULTS.md`
    - Config keys documented authoritatively (Phases 1–5: gain/reverb/
      chorus, attack_ms/release_ms, stop, coupler/sub_octave, drone/
      drone_level): see `docs/HARMONIUM_CONFIG.md`

6. **NaadCoreCLI** (`apps/naadcore-cli/main.cpp`)
   - Parses `--plugin`, `--midi`, `--audio-driver`, `--help`
   - Converts ALSA events to `MidiEvent` and routes them via PluginManager

### Architecture Flow

```
Q49 MIDI (client 20:0)
        ↓
ALSA Sequencer (via snd_seq)
        ↓
MidiInput::process_events()
        ↓
Callback (lambda in naadcore-cli main loop)
        ↓
PluginManager::route_midi_event()
        ↓
HarmoniumPlugin::handle_midi_event()
        ↓
FluidSynth API (fluid_synth_noteon/noteoff/cc/...)
        ↓
Audio Driver → Speakers
```

## Actual APIs Used

### FluidSynth API (v2.4.8)

From `/usr/include/fluidsynth/synth.h`:
- `new_fluid_synth(settings)` - Create synthesizer instance
- `delete_fluid_synth(synth)` - Destroy synthesizer
- `fluid_synth_sfload(synth, filename, reset)` - Load SoundFont
- `fluid_synth_noteon(synth, channel, note, velocity)` - Note on
- `fluid_synth_noteoff(synth, channel, note)` - Note off
- `new_fluid_audio_driver(settings, synth)` - Create audio driver
- `delete_fluid_audio_driver(driver)` - Destroy audio driver
- `new_fluid_settings()` / `delete_fluid_settings()` - Settings management

From `/usr/include/fluidsynth/types.h`:
- `fluid_synth_t`, `fluid_settings_t`, `fluid_audio_driver_t` - Opaque types

### ALSA Sequencer API

From `/usr/include/alsa/seq.h` and `/usr/include/alsa/seq_event.h`:
- `snd_seq_open()` - Open sequencer
- `snd_seq_close()` - Close sequencer
- `snd_seq_set_client_name()` - Set client name ("naadcore")
- `snd_seq_create_port()` - Create MIDI port
- `snd_seq_port_info_set_*()` - Configure port
- `snd_seq_subscribe_port()` - Subscribe to MIDI source
- `snd_seq_event_input()` - Read events (non-blocking)
- `snd_seq_client_id()` - Get client ID

Event Types:
- `SND_SEQ_EVENT_NOTEON` - Note on with velocity
- `SND_SEQ_EVENT_NOTEOFF` - Note off

Event Structure (`snd_seq_event_t`):
- `type` - Event type
- `data.note` - Note event data (channel, note, velocity)

## Build Instructions

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install g++ cmake libfluidsynth-dev libasound2-dev

# Verify FluidSynth version
fluidsynth --version  # Should be 2.4.8
```

### Building

```bash
cd /home/nil/Projects/Personal/naadcore
cmake -B build
cmake --build build -j4
```

### Build Output

- Executable: `naadcore/build/apps/naadcore-cli/naadcore-cli`
- Plugin: `naadcore/build/plugins/libharmonium_plugin.so`
- Library: `naadcore/build/libnaadcore_core.so` (shared)

## Usage

### Command Line

```bash
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0
```

### Options

- `--plugin <path>` - Path to plugin shared library (.so) (required)
- `--midi <client:port>` - ALSA sequencer client:port (e.g., "20:0")
- `--audio-driver <name>` - Audio driver: alsa, pipewire, pulseaudio
- `--help` - Show usage information

There is no `--soundfont` flag; the SoundFont path is compiled into the plugin
(see `plugins/harmonium/CMakeLists.txt`, `HARMONIUM_SOUNDFONT_PATH`).

### Q49 MIDI Setup

1. Verify Q49 is detected by ALSA:
   ```bash
   aconnect -o
   # Should show: client 20: 'Q49' [type=kernel,card=1]
   ```

2. Start the CLI — it subscribes to 20:0 automatically. Verify:
   ```bash
   aconnect -l
   # Look for: client XXX: 'naadcore'
   #   0 'naadcore input'   Connected From: 20:0
   ```

3. Play notes on Q49 - they should trigger harmonium sounds

If needed, connect manually: `aconnect 20:0 <client>:0`.

## Known Limitations

### Current Limitations (Future Work)

1. **Single plugin per run** - CLI accepts one `--plugin` flag
2. **No plugin configuration** - `set_config/get_config` not wired to CLI
3. **Timestamp not populated** - `MidiEvent::timestamp` stays 0
4. **No raga/notation/sargam** - Pure MIDI interface
5. **Partial bellows modeling** - Uniform chord velocity implemented (plugin-local); pressure/expression (CC#11) not modeled
6. **No GUI/visualizer** - Terminal-only interface

### Technical Limitations

1. **Memory management** - Manual new/delete (could use smart pointers)
2. **Error handling** - Basic error messages, no recovery
3. **Threading** - Single-threaded MIDI loop with usleep(1000)
4. **No latency optimization** - Could use poll() instead of sleep

## Testing

### Realism test harness (tests/)

The harmonium realism effort has a dedicated harness under `tests/` — see
`tests/README.md`. It provides 7 standard MIDI test tracks (T1–T7: envelope,
legato, chords, staccato, drone, repertoire phrase, velocity sweep), offline
FluidSynth rendering, live capture through the full CLI → plugin → audio
chain (PipeWire `parecord --monitor-stream` on this machine), objective WAV
analysis (`tests/analyze.py`), and an A/B score sheet (`tests/RESULTS.md`).
Config-seam unit tests: `tests/scripts/run_config_tests.sh` (213 checks).

### Verify Build

```bash
cmake -B build && cmake --build build -j4
./build/apps/naadcore-cli/naadcore-cli --help
```

### Test MIDI Input

```bash
# Start the CLI in background (from the project root)
./build/apps/naadcore-cli/naadcore-cli \
    --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0 &

# In another terminal, check MIDI connections
aconnect -l

# Send a test note by hand (this aseqsend build uses positional hex syntax)
aseqsend -p <client>:0 "90 60 100"   # note on
aseqsend -p <client>:0 "80 60 0"     # note off

# Press keys on Q49 - should hear harmonium sounds
```

## Technical Notes

### ALSA Sequencer Subscription

The CLI's MidiInput creates a WRITE-capable port (so others can send events into
it) and subscribes to the source port:

```cpp
// Open sequencer
snd_seq_open(&seq_, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);

// Create port with SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE
snd_seq_create_port(seq_, port_info);

// Subscribe to source
snd_seq_port_subscribe_t* sub;
snd_seq_port_subscribe_set_sender(sub, &source);  // client:port to listen to
snd_seq_port_subscribe_set_dest(sub, &dest);      // our port
snd_seq_subscribe_port(seq_, sub);
```

Note: READ capabilities would make the port an event *source*, which is why an
early bug prevented event delivery despite the subscription appearing in
`aconnect -l`.

### FluidSynth Audio Driver

The audio driver is set explicitly ("alsa" by default) because auto-selection can
pick a broken driver on some systems (e.g. an uninitialized SDL3 build), which
results in silence. `--audio-driver pulseaudio` is a working fallback; the
native `pipewire` driver fails on this machine's FluidSynth 2.4.8 build
(missing `pw_init()`) — the default `alsa` and `pulseaudio` drivers are
both proxied by PipeWire anyway.

### MIDI Event Conversion (CLI)

```cpp
// In the MIDI callback:
switch (ev.type) {
    case SND_SEQ_EVENT_NOTEON:
        event.type = MidiEvent::NOTE_ON;  // velocity 0 handled as note off by plugin
        ...
    case SND_SEQ_EVENT_NOTEOFF:
        event.type = MidiEvent::NOTE_OFF;
        ...
}
pm.route_midi_event(event);   // fan-out to all loaded plugins
```

## Next Steps

The harmonium realism effort (Phases 0–6) is COMPLETE (2026-09-19) —
see `HANDOVER.md`. Items 3–6 below are done (preset selection = `stop`
key; coupler/sub-octave = Phase 4; CC depth partial = bend/CC11
mirroring); remaining:

1. **Other instrument plugins** - pipe organ, drone/tanpura, etc. using the
   same INaadPlugin contract (active focus)
2. **Multiple plugins** - Accept several plugins per CLI run (needs the
   host-owned-audio / shared-graph decision first)
3. **Raga Support** - Sargam notation and microtonal tuning in the harmonium plugin
4. **GUI/Visualizer** - Qt-based interface
5. **Performance Optimization** - Use poll() instead of sleep

## Documentation

- `README.md` (root) - Build/run instructions
- `HANDOVER.md` - Authoritative current-state handover
- `docs/NAADCORE_ARCHITECTURE.md` - Architecture overview
- `docs/PLUGIN_SYSTEM.md` - Plugin system documentation
- `docs/PLUGIN_DEVELOPMENT.md` - Plugin development guide
- `docs/HARMONIUM_CONFIG.md` - Harmonium plugin config-key registry
- `docs/HARMONIUM_SF2_AUDIT.md` - harmonium.sf2 structure audit (Phase 1 realism work)
- `tests/README.md` - Realism test harness guide
- `tests/RESULTS.md` - Objective + subjective A/B results
- `docs/NAADCORE_MVP_CHALLENGE.md` - Historical MVP record (completed)