# 🎵 NAADCORE MVP - CHALLENGE AND RESOLUTION

> **STATUS: COMPLETED — historical document.**
> The MVP described below was delivered (Q49 → ALSA → FluidSynth → harmonium sound)
> and was later superseded by the plugin architecture: the `naadcore-harmonium`
> MVP binary has been removed, and harmonium sound is now delivered by the
> `harmonium` plugin loaded through `naadcore-cli` (see `HANDOVER.md` and
> `docs/NAADCORE_ARCHITECTURE.md`). The plugin-system phases sketched in the second
> half of this document have all been implemented. The content below is preserved
> as a record of the original design challenge and is not maintained.

## ORIGINAL MVP DESIGN

### Initial Requirements
- Linux-native digital harmonium synthesis
- FluidSynth 2.4.8 as synthesizer backend
- Alesis Q49 MIDI input via ALSA sequencer
- No sfizz (not available), no JACK, no GUI
- MVP only: note on/off, polyphony, no raga/coupler/bellows/GUI/CC

### Initial Challenge
User challenged the architect-planner to simplify the design for a true MVP.

### Simplified MVP Design (architect-planner response)

**Key Decision:** Use FluidSynth 2.4.8 directly instead of sfizz (which wasn't installed).

**MVP Architecture:**
```
Q49 → ALSA MIDI → NaadCore (FluidSynth wrapper) → harmonium.sf2 → ALSA/PipeWire → speakers
```

**CLI Interface:**
```
./naadcore-harmonium --soundfont <path> --midi 20:0
```

**Implementation:**
- `core/midi.hpp/cpp`: MidiInput (ALSA) + Synthesizer (FluidSynth)
- `apps/harmonium.cpp`: CLI with argument parsing
- CMakeLists.txt: Build system with pkg-config

**Resolved Bugs:**
1. MIDI event delivery (port capabilities READ→WRITE)
2. Audio driver selection (explicit "alsa" instead of auto)
3. Event processing (drain all pending events)
4. Port subscription (use parsed port number)

**Status: ✅ MVP Complete and Working**
- Alesis Q49 → ALSA sequencer → NaadCore → FluidSynth 2.4.8 → harmonium.sf2 → speakers
- Press Q49 keys → hear harmonium sound

---

## FUTURE: PLUGIN SYSTEM

### What Changed After MVP
User requested separation of core from harmonium to enable plugins.

### Plugin Architecture Decision

**Approach:** Dynamic plugin system using C++17 ABIs

**Key Features:**
- Plugins built as separate .so files
- Core library provides plugin management infrastructure
- Harmonium is just one plugin (harmonium.so)
- Easy to add new plugins (pipe_organ.so, synth.so, etc.)

**Plugin Interface:**
```cpp
class INaadPlugin {
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual std::string get_name() const = 0;
    virtual void handle_midi_event(const MidiEvent&) = 0;
    virtual void process_audio(float** output, int frames) = 0;
};
```

### Build System Changes
```
naadcore/
├── CMakeLists.txt          # Core library
├── plugins/
│   ├── CMakeLists.txt
│   └── harmonium/
│       └── harmonium_plugin.cpp
├── apps/
│   └── naadcore-cli/       # Plugin loader (replaces harmonium app)
└── core/
    ├── midi.cpp
    └── plugin_manager.cpp  # NEW: Plugin management
```

### Migration Path

**Phase 1 (Now):** Core infrastructure
- Create plugin interface (naadcore/plugin.hpp)
- Implement PluginManager
- Update CMakeLists.txt

**Phase 2:** Extract harmonium to plugin
- Create plugins/harmonium/harmonium_plugin.cpp
- Migrate logic from apps/harmonium.cpp
- Implement plugin factory functions

**Phase 3:** Create CLI loader
- Rewrite apps/harmonium.cpp → apps/naadcore-cli/main.cpp
- Load plugins dynamically
- Route MIDI to plugins

### Current Status

✅ **MVP Complete**: Harmonium synthesizer working
✅ **Plugin Architecture Designed**: Dynamic .so plugin system
⏳ **Plugin Implementation Pending**: Need to extract harmonium to plugin

### User Workflow (After Plugin System)

```
# Build plugins
cd build && make

# List available plugins
./naadcore-cli --list-plugins

# Run with specific plugin
./naadcore-cli --plugin harmonium --soundfont ~/harmonium-companion/harmonium.sf2 --midi 20:0

# Run with different plugin (future)
./naadcore-cli --plugin pipe_organ --soundfont ~/pipe_organ.sf2 --midi 20:0
```

### Next Steps

1. Implement plugin interface in core
2. Create harmonium plugin
3. Update CLI application
4. Test with actual hardware