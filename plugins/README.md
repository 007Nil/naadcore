# Plugins Directory

This directory contains plugin implementations for NaadCore.

For detailed plugin development guidance, see `docs/PLUGIN_DEVELOPMENT.md`.

## Available Plugins

### harmonium/
The harmonium plugin provides a FluidSynth-based harmonium synthesizer with an embedded SoundFont.

**Build Output**: `libharmonium_plugin.so` in `build/plugins/`

**Usage** (from the project root):
```bash
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0
```

## Adding New Plugins

1. Create a new directory for your plugin (e.g., `synth/`)
2. Implement the `INaadPlugin` interface
3. Create `CMakeLists.txt` following the pattern in `harmonium/`
4. Add `add_subdirectory()` to the main `CMakeLists.txt`

## Plugin Structure

Each plugin should have:
- Plugin header: `plugin.hpp`
- Plugin implementation: `plugin.cpp`
- CMakeLists.txt for building
- Embedded SoundFont path (compile-time constant)

## Build Process

When you build the project, all plugins listed in `add_subdirectory()` will be compiled:
```bash
cmake -B build
cmake --build build -j4
```

## Plugin Lifecycle

1. **Load**: Plugin shared library is loaded via `dlopen()`
2. **Create**: `naad_plugin_create()` is called to instantiate the plugin
3. **Init**: `init()` is called to initialize the synthesizer
4. **Start**: `start_audio()` starts the audio driver
5. **Process**: `handle_midi_event()` processes incoming MIDI events
6. **Stop**: `stop_audio()` stops the audio driver
7. **Unload**: `naad_plugin_destroy()` destroys the plugin instance
8. **Unload Library**: `dlclose()` unloads the shared library

## Key Requirements

- Plugins must export `naad_plugin_create()`, `naad_plugin_destroy()`, and `naad_plugin_get_version()`
- SoundFonts are embedded at compile time using `target_compile_definitions()`
- Plugins use the `naadcore_core` library for MIDI input handling
