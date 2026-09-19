# Plugin System Implementation Summary

## Overview

The plugin system for NaadCore has been successfully implemented and is fully functional. This document describes the current working implementation (not future plans).

## Current Implementation Status

**Working Features:**
- ✅ Plugin interface (`INaadPlugin`) defined in `include/naadcore/plugin.hpp`
- ✅ Plugin manager singleton (`PluginManager`) in `include/naadcore/plugin_manager.hpp` + `core/plugin_manager.cpp`
- ✅ Dynamic plugin loading via `dlopen()` with `RTLD_NOW | RTLD_LOCAL`
- ✅ Harmonium plugin implementation at `plugins/harmonium/`
- ✅ CLI application (`naadcore-cli`) for loading plugins
- ✅ Embedded SoundFont support (compile-time path)
- ✅ Full MIDI event handling (Note On/Off, CC, Pitch Bend, etc.)

**Not Implemented (Future):**
- Multiple simultaneous plugins
- Plugin configuration via JSON/YAML
- Plugin discovery mechanism
- Plugin management CLI commands
- MIDI routing between plugins

## Files Structure

### Core Files

1. **`include/naadcore/plugin.hpp`** - Plugin interface definition
   - `INaadPlugin` abstract interface
   - `MidiEvent` struct for cross-platform MIDI events
   - `PluginInfo` struct for plugin metadata
   - `PluginResult` enum for return codes
   - C-linkage factory function typedefs

2. **`include/naadcore/plugin_manager.hpp` + `core/plugin_manager.cpp`** - Plugin manager implementation
   - `PluginManager` singleton class
   - `load_plugin(path)` - Load plugin from shared library
   - `unload_plugin(path)` - Unload plugin
   - `route_midi_event(event)` - Route MIDI events to plugins
   - Thread-safe with mutex protection
   - Dynamic loading via `dlopen()` with `RTLD_NOW | RTLD_LOCAL`

3. **`include/naadcore/midi.hpp`** + `core/midi.cpp` - MIDI input and Synthesizer implementations

### Plugin Implementation

4. **`plugins/harmonium/`** - Harmonium plugin directory
   - `CMakeLists.txt` - Build configuration
   - `harmonium_plugin.hpp` - Plugin header
   - `harmonium_plugin.cpp` - Plugin implementation
   - Embeds SoundFont path via `HARMONIUM_SOUNDFONT_PATH`

### CLI Application

5. **`apps/naadcore-cli/`** - Plugin loader CLI
   - `CMakeLists.txt` - Build configuration
   - `main.cpp` - CLI implementation
   - Parses `--plugin <path>` and `--midi <client:port>` flags
   - Routes MIDI events to loaded plugins

### Build System

6. **`CMakeLists.txt`** - Root build file
   - Added `naadcore_core` as SHARED library
   - Added `plugins/harmonium` subdirectory
   - Added `apps/naadcore-cli` subdirectory
   - Install rules for all targets

## Build Output

```
build/
├── libnaadcore_core.so        # Core library (SHARED)
├── apps/
│   └── naadcore-cli/
│       └── naadcore-cli       # Plugin loader CLI
└── plugins/
    └── libharmonium_plugin.so # Harmonium plugin
```

## CLI Usage

```bash
# Basic usage (from the project root)
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0

# With audio driver override
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0 --audio-driver pipewire

# Show help
./build/apps/naadcore-cli/naadcore-cli --help
```

## Plugin API

All plugins must implement:
- `INaadPlugin` interface (from `naadcore/plugin.hpp`)
- Export C-linkage functions:
  - `naad_plugin_create()` - Creates plugin instance
  - `naad_plugin_destroy()` - Destroys plugin instance
  - `naad_plugin_get_version()` - Returns API version

## SoundFont Embedding

The SoundFont path is a compile-time constant (actual mechanism from
`plugins/harmonium/CMakeLists.txt`; the default is the committed in-repo
derived font, so the plugin is self-contained):

```cmake
if(NOT HARMONIUM_SOUNDFONT_PATH)
    set(HARMONIUM_SOUNDFONT_PATH
        "${CMAKE_SOURCE_DIR}/plugins/harmonium/soundfonts/harmonium_v3.sf2")
endif()

target_compile_definitions(harmonium_plugin PRIVATE
    HARMONIUM_SOUNDFONT_PATH="${HARMONIUM_SOUNDFONT_PATH}"
)
```

To change the SoundFont path:

```bash
cmake -B build -DHARMONIUM_SOUNDFONT_PATH=/path/to/soundfont.sf2
cmake --build build -j4
```

## Key Design Decisions

1. **Local Plugins**: Plugins are project-local, not system-wide
2. **Embedded SoundFonts**: SoundFonts compiled into plugins, not loaded at runtime
3. **Explicit Plugin Path**: `--plugin <path>` required
4. **C-Linkage Exports**: ABI-compatible for dynamic loading
5. **Thread Safety**: PluginManager uses mutex for thread-safe operations
6. **Single Plugin Mode**: Currently loads one plugin at a time

## Testing

```bash
# Verify plugin exports
nm -D ./build/plugins/libharmonium_plugin.so | grep naad_plugin

# Run CLI
./build/apps/naadcore-cli/naadcore-cli --help

# Load and test plugin (requires SoundFont)
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0
```

## Documentation

- `docs/PLUGIN_SYSTEM.md` - Detailed plugin system documentation
- `docs/PLUGIN_DEVELOPMENT.md` - Plugin development guide
- `docs/NAADCORE_ARCHITECTURE.md` - Architecture overview

## Files Reference

### Header Files (in `include/naadcore/`)
- `plugin.hpp` - Plugin interface
- `midi.hpp` - MIDI input definitions

### Core Files (in `core/`)
- `midi.cpp` - MIDI implementation
- `plugin_manager.cpp` - Plugin manager implementation

### Plugin Files (in `plugins/harmonium/`)
- `CMakeLists.txt` - Plugin build config
- `harmonium_plugin.hpp` - Plugin header
- `harmonium_plugin.cpp` - Plugin implementation

### CLI Files (in `apps/naadcore-cli/`)
- `CMakeLists.txt` - CLI build config
- `main.cpp` - CLI implementation

## Notes

- The plugin system is designed for local, project-specific plugins
- Plugins are loaded with `RTLD_NOW | RTLD_LOCAL` for immediate symbol resolution
- The SoundFont path must be set at compile time (not runtime)
- MIDI events are converted from ALSA format to plugin format internally
- Currently supports single plugin loading (multiple plugins planned for future)
