# Plugin Development Guide

## Overview

This guide provides a comprehensive walkthrough for developing plugins for NaadCore. Plugins are dynamically loadable shared libraries that implement the `INaadPlugin` interface and handle their own audio synthesis.

## Prerequisites

- C++17 compiler (GCC 7+ or Clang 5+)
- CMake 3.16+
- FluidSynth 2.0+
- ALSA libraries

## Plugin Architecture

A NaadCore plugin consists of:

1. **Plugin Implementation**: A C++ class implementing `INaadPlugin`
2. **C-Linkage Factory Functions**: Exports for creating/destroying plugin instances
3. **Build Configuration**: CMakeLists.txt for building the shared library
4. **SoundFont**: Embedded at compile time via preprocessor definition

### Plugin File Structure

```
plugins/my_plugin/
├── CMakeLists.txt
├── my_plugin.hpp
└── my_plugin.cpp
```

## Creating a Plugin

### Step 1: Implement the Plugin Interface

Create `my_plugin.hpp`:

```cpp
#ifndef MY_PLUGIN_HPP
#define MY_PLUGIN_HPP

#include <naadcore/plugin.hpp>

class MyPlugin : public naadcore::INaadPlugin {
public:
    MyPlugin();
    ~MyPlugin() override;
    
    const PluginInfo* get_info() override;
    PluginResult init(const char* audio_driver = nullptr) override;
    PluginResult start_audio() override;
    PluginResult stop_audio() override;
    PluginResult handle_midi_event(const naadcore::MidiEvent& event) override;
    std::string get_config(const char* key) override;
    PluginResult set_config(const char* key, const char* value) override;

private:
    // Your plugin's internal state
    std::string audio_driver_;
    std::string audio_device_;  // well-known environment key, see below
    void* synthesizer_;  // Replace with your actual synthesizer
};

#endif
```

### Step 2: Implement Plugin Methods

Create `my_plugin.cpp`:

```cpp
#include "my_plugin.hpp"
#include <iostream>

// SoundFont path is embedded at compile time
#ifndef MY_PLUGIN_SOUNDFONT_PATH
#define MY_PLUGIN_SOUNDFONT_PATH "/path/to/soundfont.sf2"
#endif

namespace naadcore {

MyPlugin::MyPlugin() : synthesizer_(nullptr), audio_driver_("alsa") {
}

MyPlugin::~MyPlugin() {
    stop_audio();
    // Clean up synthesizer resources
}

const PluginInfo* MyPlugin::get_info() {
    static PluginInfo info = {
        "my_plugin",
        "1.0.0",
        "Your Name",
        "Description of my plugin"
    };
    return &info;
}

PluginResult MyPlugin::init(const char* audio_driver) {
    if (audio_driver) {
        audio_driver_ = audio_driver;
    }
    
    // Initialize your synthesizer here
    // Load SoundFont from embedded path
    
    // Environment keys (see "Environment keys every plugin SHOULD
    // support" below): the device string must be in your synth settings
    // BEFORE you create the audio driver (start_audio). Per-driver
    // mapping, e.g. for FluidSynth:
    if (!audio_device_.empty()) {
        if (audio_driver_ == "alsa") {
            fluid_settings_setstr(settings_, "audio.alsa.device",
                                  audio_device_.c_str());
        } else if (audio_driver_ == "pulseaudio") {
            fluid_settings_setstr(settings_, "audio.pulseaudio.device",
                                  audio_device_.c_str());
        }
    }
    
    std::cout << "Initializing MyPlugin with audio driver: " << audio_driver_ << std::endl;
    
    return PLUGIN_OK;
}

PluginResult MyPlugin::start_audio() {
    // Start your audio driver
    std::cout << "Starting audio with driver: " << audio_driver_ << std::endl;
    return PLUGIN_OK;
}

PluginResult MyPlugin::stop_audio() {
    // Stop your audio driver
    std::cout << "Stopping audio" << std::endl;
    return PLUGIN_OK;
}

PluginResult MyPlugin::handle_midi_event(const MidiEvent& event) {
    // Process MIDI events based on type
    switch (event.type) {
        case MidiEvent::NOTE_ON:
            // Handle note on
            break;
        case MidiEvent::NOTE_OFF:
            // Handle note off
            break;
        case MidiEvent::CONTROL_CHANGE:
            // Handle control change
            break;
        case MidiEvent::PITCH_BEND:
            // Handle pitch bend
            break;
        case MidiEvent::PROGRAM_CHANGE:
            // Handle program change
            break;
        case MidiEvent::CHANNEL_PRESSURE:
            // Handle channel pressure
            break;
        case MidiEvent::KEY_PRESSURE:
            // Handle key pressure
            break;
        default:
            // Unknown event type
            return PLUGIN_ERROR;
    }
    return PLUGIN_OK;
}

std::string MyPlugin::get_config(const char* key) {
    if (!key) return "";
    
    std::string k = key;
    if (k == "audio_driver") {
        return audio_driver_;
    }
    if (k == "audio_device") {
        return audio_device_;  // "" = unset (plugin default device)
    }
    
    return "";
}

PluginResult MyPlugin::set_config(const char* key, const char* value) {
    if (!key || !value) {
        return PLUGIN_INVALID_PARAM;
    }
    
    std::string k = key;
    if (k == "audio_driver") {
        audio_driver_ = value;
        return PLUGIN_OK;
    }
    if (k == "audio_device") {
        // Well-known environment key (the host applies it BEFORE init()).
        // Store verbatim — NO validation at set time (device names are
        // machine-specific); an unusable device fails start_audio().
        // Empty string = unset (plugin default device).
        audio_device_ = value;
        return PLUGIN_OK;
    }
    
    return PLUGIN_NOT_IMPLEMENTED;
}

} // namespace naadcore

// ============================================================================
// C-linkage factory functions (exported by plugin)
// ============================================================================

extern "C" {

naadcore::INaadPlugin* naad_plugin_create() {
    return new naadcore::MyPlugin();
}

void naad_plugin_destroy(naadcore::INaadPlugin* plugin) {
    delete plugin;
}

int naad_plugin_get_version() {
    return 1;
}

} // extern "C"
```

### Step 3: Create CMakeLists.txt

Create `CMakeLists.txt` in your plugin directory:

```cmake
# Build my_plugin as shared library

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${FLUIDSYNTH_INCLUDE_DIRS})

# Create plugin as SHARED library
add_library(my_plugin SHARED
    ${CMAKE_SOURCE_DIR}/plugins/my_plugin/my_plugin.cpp
)

# SoundFont path (can be overridden via -DMY_PLUGIN_SOUNDFONT_PATH=... during cmake)
if(NOT MY_PLUGIN_SOUNDFONT_PATH)
    set(MY_PLUGIN_SOUNDFONT_PATH "${CMAKE_SOURCE_DIR}/plugins/my_plugin/default.sf2")
endif()

target_compile_definitions(my_plugin PRIVATE
    MY_PLUGIN_SOUNDFONT_PATH="${MY_PLUGIN_SOUNDFONT_PATH}"
)

target_link_libraries(my_plugin
    naadcore_core
    ${FLUIDSYNTH_LIBRARIES}
)

set_target_properties(my_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    POSITION_INDEPENDENT_CODE ON
)

# Install rules
install(TARGETS my_plugin
    LIBRARY DESTINATION lib/naadcore/plugins
)
```

### Step 4: Register Plugin in Main CMakeLists.txt

Add your plugin to the main `CMakeLists.txt`:

```cmake
# Add harmonium plugin (already present)
add_subdirectory(${CMAKE_SOURCE_DIR}/plugins/harmonium)

# Add your plugin
add_subdirectory(${CMAKE_SOURCE_DIR}/plugins/my_plugin)
```

## Building the Plugin

### Full Project Build

```bash
cd /home/nil/Projects/Personal/naadcore
cmake -B build
cmake --build build -j4
```

### Build Only Your Plugin

```bash
cmake --build build --target my_plugin
```

The plugin will be built to:
```
build/plugins/libmy_plugin.so
```

## Using Your Plugin

### Command Line

```bash
# Basic usage (from the project root)
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libmy_plugin.so --midi 20:0

# With audio driver override
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libmy_plugin.so --midi 20:0 --audio-driver pipewire

# With an explicit audio output device (driver-specific: ALSA PCM name or
# PulseAudio sink name; unset = the plugin's default device). Applies at
# start — restart the CLI to change it.
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libmy_plugin.so --midi 20:0 --audio-driver alsa --audio-device default

# Show help
./build/apps/naadcore-cli/naadcore-cli --help
```

### Verify Plugin Exports

```bash
# Check that required functions are exported
nm -D ./build/plugins/libmy_plugin.so | grep naad_plugin
```

Expected output:
```
0000000000001234 T naad_plugin_create
0000000000001256 T naad_plugin_destroy
0000000000001278 T naad_plugin_get_version
```

## Plugin Interface Reference

### Environment keys every plugin SHOULD support

The host hands the user's audio environment to every plugin through the
config seam (`set_config`) **BEFORE** `init()`. These keys are optional
capabilities, not ABI — `naad_plugin_get_version()` does NOT bump for them.

| Key | Meaning | Typical backing setting (FluidSynth) |
|---|---|---|
| `audio_driver` | Audio driver name (`alsa`, `pipewire`, `pulseaudio`, `file`, ...) | `audio.driver` |
| `audio_device` | Audio output device name (an ALSA PCM name, a PulseAudio sink name, ...) | `audio.alsa.device` / `audio.pulseaudio.device` |

Contract:

- **Honor them or return `PLUGIN_NOT_IMPLEMENTED`** — both are acceptable;
  the host treats a non-OK, non-`PLUGIN_NOT_IMPLEMENTED` result as a warning
  only and never fails the load.
- **Timing rule:** the host applies both keys via `set_config` before
  `init()`. For FluidSynth-based plugins the device string must be in the
  `fluid_settings` **before** `new_fluid_audio_driver` is called (i.e. map
  it in `init()`, per driver — see the `init()` snippet in Step 2 above).
  The audio driver itself is created at `start_audio()`.
- **No validation at set time.** Store the value verbatim; device names are
  machine-specific and the real validator is `start_audio()`, where a bad
  device fails driver creation with `PLUGIN_ERROR` and a clear message.
  `""` = unset → the plugin's own default device.
- **Applies at start only.** A post-init `set_config("audio_device")` is
  stored for the NEXT run; the running driver is never restarted (that
  would drone sustains).
- **Multi-plugin note:** with several plugins loaded at once, a
  `default` PipeWire/PulseAudio sink accepts multiple plugin streams
  (they mix in the PipeWire graph), but a raw hardware device
  (`hw:...`/`plughw:...`) is exclusive — a second plugin would fail to
  open it. Prefer sink-level devices when fanning out.

### INaadPlugin Interface

```cpp
class INaadPlugin {
public:
    virtual const PluginInfo* get_info() = 0;
    virtual PluginResult init(const char* audio_driver = nullptr) = 0;
    virtual PluginResult start_audio() = 0;
    virtual PluginResult stop_audio() = 0;
    virtual PluginResult handle_midi_event(const MidiEvent& event) = 0;
    virtual std::string get_config(const char* key) = 0;
    virtual PluginResult set_config(const char* key, const char* value) = 0;
};
```

### PluginInfo Structure

```cpp
struct PluginInfo {
    const char* name;           // Plugin name
    const char* version;        // Plugin version
    const char* author;         // Plugin author
    const char* description;    // Plugin description
};
```

### PluginResult Enum

```cpp
enum PluginResult {
    PLUGIN_OK = 0,
    PLUGIN_ERROR = -1,
    PLUGIN_NOT_IMPLEMENTED = -2,
    PLUGIN_INVALID_PARAM = -3
};
```

### MidiEvent Structure

```cpp
struct MidiEvent {
    enum Type {
        NOTE_ON = 1,
        NOTE_OFF = 2,
        CONTROL_CHANGE = 3,
        PITCH_BEND = 4,
        PROGRAM_CHANGE = 5,
        CHANNEL_PRESSURE = 6,
        KEY_PRESSURE = 7
    };
    
    Type type;              // Event type
    uint8_t channel;        // MIDI channel (0-15)
    uint8_t data1;          // First data byte
    uint8_t data2;          // Second data byte
    uint64_t timestamp;     // Event timestamp in microseconds
};
```

## Debugging Plugins

### Common Issues

1. **Plugin fails to load**
   - Check that `naad_plugin_create` is exported
   - Verify dependencies are available: `ldd libmy_plugin.so`
   - Check plugin binary is a valid shared library: `file libmy_plugin.so`

2. **Plugin loads but initialization fails**
   - Verify SoundFont path is correct
   - Check audio driver name is valid
   - Look for error messages in console output

3. **No audio output**
   - Verify MIDI events are being received
   - Check audio driver is running
   - Verify SoundFont loaded successfully

### Verification Commands

```bash
# Check plugin exports
nm -D ./build/plugins/libmy_plugin.so | grep naad_plugin

# Check dependencies
ldd ./build/plugins/libmy_plugin.so

# Verify plugin is a shared library
file ./build/plugins/libmy_plugin.so

# Run with verbose output
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libmy_plugin.so --midi 20:0
```

## Harmonium Plugin Example

See `plugins/harmonium/harmonium_plugin.cpp` for a complete example of a working plugin that uses FluidSynth.

## Building Without SoundFont

If you don't have a SoundFont yet, you can create a minimal plugin that handles MIDI events without loading a SoundFont:

```cpp
PluginResult MyPlugin::init(const char* audio_driver) {
    if (audio_driver) {
        audio_driver_ = audio_driver;
    }
    
    // Create synthesizer without loading SoundFont
    synthesizer_ = create_synthesizer(audio_driver_);
    
    std::cout << "Initializing MyPlugin (no SoundFont)" << std::endl;
    return PLUGIN_OK;
}
```

## Next Steps

1. Test your plugin with a simple synthesizer
2. Add SoundFont support for full functionality
3. Implement additional MIDI event handlers
4. Add plugin-specific configuration options
5. Create documentation for your plugin

## Resources

- `include/naadcore/plugin.hpp` - Plugin interface definition
- `plugins/harmonium/` - Harmonium plugin implementation
- `include/naadcore/plugin_manager.hpp` + `core/plugin_manager.cpp` - Plugin manager implementation
