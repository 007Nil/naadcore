# NaadCore Architecture

## Overview

NaadCore is a modular synthesizer framework that supports plugin-based audio engines. The system consists of a core library, plugin manager, CLI interface, and plugin implementations. Plugins are dynamically loaded shared libraries that implement the `INaadPlugin` interface.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                            CLI Application                            │
│  (naadcore-cli - Plugin loader, MIDI event processor)                │
└───────────────────────┬─────────────────────────────────────────────┘
                        │
                        │ ┌──────────────────────────────────────────┐
                        │ │ Plugin Manager (Singleton)               │ │
                        │ │ - Plugin loading/unloading               │ │
                        │ │ - MIDI event routing                     │ │
                        │ │ - Lifecycle management                   │ │
                        │ └─────────────────┬────────────────────────┘ │
                        │                   │                          │
                        ▼                   ▼                          ▼
              ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
              │ Harmonium    │  │ Custom     │  │ Custom       │
              │ Plugin       │  │ Plugin A   │  │ Plugin B     │
              │              │  │            │  │              │
              │ - SoundFont  │  │ - Custom   │  │ - Custom     │
              │   embedded   │  │   logic    │  │   logic      │
              │ - FluidSynth │  │ - Different│  │ - Different  │
              │   synth      │  │   synth    │  │   synth      │
              └──────────────┘  └──────────────┘  └──────────────┘
```

## Components

### 1. Core Library (`naadcore_core`)

The core library provides the foundational functionality:

- **MIDI Input** (`include/naadcore/midi.hpp`, `core/midi.cpp`): ALSA sequencer integration
- **Plugin Manager** (`include/naadcore/plugin_manager.hpp`, `core/plugin_manager.cpp`): Plugin lifecycle management
- **Shared Infrastructure**: Common types used by plugins

**API**:
- `MidiInput`: ALSA sequencer client for receiving MIDI events
- `PluginManager`: Singleton for loading/unloading plugins and routing events

### 2. Plugin Interface (`include/naadcore/plugin.hpp`)

The plugin interface defines the contract for all plugins:

```cpp
class INaadPlugin {
    virtual const PluginInfo* get_info() = 0;
    virtual PluginResult init(const char* audio_driver = nullptr) = 0;
    virtual PluginResult start_audio() = 0;
    virtual PluginResult stop_audio() = 0;
    virtual PluginResult handle_midi_event(const MidiEvent& event) = 0;
    virtual std::string get_config(const char* key) = 0;
    virtual PluginResult set_config(const char* key, const char* value) = 0;
};
```

**Required exports**:
- `naad_plugin_create()`: Creates plugin instance
- `naad_plugin_destroy()`: Destroys plugin instance
- `naad_plugin_get_version()`: Returns API version

### 3. Plugin Manager (`include/naadcore/plugin_manager.hpp`, `core/plugin_manager.cpp`)

The PluginManager is a singleton that manages all plugin operations:

```cpp
class PluginManager {
    static PluginManager& instance();
    
    PluginResult load_plugin(const std::string& path);
    PluginResult unload_plugin(const std::string& path);
    PluginResult route_midi_event(const MidiEvent& event);
    
    PluginResult start_all_audio();
    PluginResult stop_all_audio();
    void cleanup();
};
```

**Features**:
- Dynamic loading via `dlopen()` with `RTLD_NOW | RTLD_LOCAL`
- Thread-safe operation with mutex protection
- Plugin lifecycle management
- Event routing to all loaded plugins

### 4. CLI Application (`apps/naadcore-cli/`)

The CLI application loads plugins and processes MIDI events:

```bash
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi <client:port> [options]
```

**Options**:
- `--plugin <path>`: Path to plugin shared library (required)
- `--midi <client:port>`: ALSA sequencer client:port (e.g., "20:0")
- `--audio-driver <name>`: Audio driver (alsa, pipewire, pulseaudio)
- `--help`: Show usage information

### 5. Harmonium Plugin (`plugins/harmonium/`)

The harmonium plugin is a reference implementation using FluidSynth:

**Features**:
- FluidSynth-based synthesizer
- Embedded SoundFont path (compile-time constant)
- Full MIDI event handling (Note On/Off, CC, Pitch Bend, etc.)

**SoundFont Embedding**:
```cmake
# plugins/harmonium/CMakeLists.txt (actual mechanism; default = the
# committed in-repo font — the plugin is self-contained)
if(NOT HARMONIUM_SOUNDFONT_PATH)
    set(HARMONIUM_SOUNDFONT_PATH
        "${CMAKE_SOURCE_DIR}/plugins/harmonium/soundfonts/harmonium_v3.sf2")
endif()

target_compile_definitions(harmonium_plugin PRIVATE
    HARMONIUM_SOUNDFONT_PATH="${HARMONIUM_SOUNDFONT_PATH}"
)
```

## Build System

### Project Structure
```
naadcore/
├── CMakeLists.txt              # Main build configuration (3 targets)
├── include/naadcore/           # Canonical public headers
│   ├── plugin.hpp
│   ├── plugin_manager.hpp
│   └── midi.hpp
├── core/                       # Core library sources
│   ├── midi.cpp
│   └── plugin_manager.cpp
├── apps/                       # Applications
│   └── naadcore-cli/           # Plugin loader CLI
│       ├── CMakeLists.txt
│       └── main.cpp
├── plugins/                    # Plugin implementations
│   ├── harmonium/
│   │   ├── CMakeLists.txt
│   │   ├── harmonium_plugin.hpp
│   │   └── harmonium_plugin.cpp
│   └── README.md
└── docs/
    ├── PLUGIN_SYSTEM.md
    └── NAADCORE_ARCHITECTURE.md
```

### Building
```bash
# From the project root
cmake -B build
cmake --build build -j4
```

### Outputs
- `build/libnaadcore_core.so` - Core library (SHARED)
- `build/apps/naadcore-cli/naadcore-cli` - Plugin loader CLI
- `build/plugins/libharmonium_plugin.so` - Harmonium plugin

## Plugin Development

### Creating a New Plugin

1. Implement the `INaadPlugin` interface
2. Export the required C-linkage functions
3. Create a `CMakeLists.txt` for the plugin
4. Register the plugin in the main `CMakeLists.txt`

### Example CMakeLists.txt
```cmake
add_library(my_plugin SHARED
    ${CMAKE_SOURCE_DIR}/plugins/my_plugin/my_plugin.cpp
)

target_compile_definitions(my_plugin PRIVATE
    MY_PLUGIN_SOUNDFONT_PATH="${CMAKE_SOURCE_DIR}/my_plugin.sf2"
)

target_link_libraries(my_plugin
    naadcore_core
    ${FLUIDSYNTH_LIBRARIES}
)
```

## MIDI Event Flow

1. User presses key on MIDI keyboard
2. ALSA sequencer receives MIDI event
3. `MidiInput` reads event from ALSA
4. Event converted to `MidiEvent` struct
5. `PluginManager::route_midi_event()` calls plugin
6. Plugin processes event with FluidSynth
7. Audio driver outputs sound

## Key Design Decisions

### Local Plugins (Not System-Wide)
- Plugins are project-local, not installed to system paths
- Users specify full path: `--plugin ./plugins/harmonium/plugin.so`
- No `--soundfont` flag for plugins (SoundFont is embedded)

### Embedded SoundFonts
- SoundFonts are compiled into plugins at build time
- Path defined via `target_compile_definitions()`
- No dynamic SoundFont loading from user-specified paths
- Ensures plugins are self-contained and portable

### C-Linkage Exports
- Plugins export C-linkage functions for ABI compatibility
- `naad_plugin_create()`, `naad_plugin_destroy()`
- Allows dynamic loading via `dlopen()`

## Running the System

### Start Harmonium Plugin
```bash
# From the project root
./build/apps/naadcore-cli/naadcore-cli \
    --plugin ./build/plugins/libharmonium_plugin.so \
    --midi 20:0 \
    --audio-driver alsa
```

The CLI registers as ALSA client `naadcore` and subscribes to the MIDI port
automatically (verify with `aconnect -l`). SoundFonts are embedded in the plugin
at build time — there is no `--soundfont` flag.

## Testing

### Verify Plugin Exports
```bash
nm -D ./build/plugins/libharmonium_plugin.so | grep naad_plugin
```

### Verify Plugin Binary
```bash
file ./build/plugins/libharmonium_plugin.so
# Should show: ELF 64-bit LSB shared object
```

### Verify Plugin Loads
```bash
# Check that plugin can be loaded
./build/apps/naadcore-cli/naadcore-cli --help
```

## Future Enhancements

- Multiple plugin support (simultaneous plugins)
- Plugin configuration via JSON/YAML
- Plugin discovery mechanism
- Plugin management CLI commands
- MIDI routing between plugins

## References

- `docs/PLUGIN_SYSTEM.md` - Detailed plugin system documentation
- `docs/PLUGIN_DEVELOPMENT.md` - Plugin development guide
- `plugins/README.md` - Plugin directory overview
- `include/naadcore/plugin_manager.hpp` + `core/plugin_manager.cpp` - Plugin manager implementation
- `include/naadcore/plugin.hpp` - Plugin interface definition
