# NaadCore Plugin System

## Overview

NaadCore uses a plugin-based architecture where audio synthesizers are implemented as dynamically loadable shared libraries. Plugins implement the `INaadPlugin` interface and handle their own SoundFont loading via compile-time embedding.

## Key Design Decisions

1. **Local Plugins**: Plugins are local to the project, not system-wide shared libraries.
2. **Embedded SoundFonts**: SoundFonts are compiled into plugins at build time, not loaded dynamically from user-specified paths.
3. **Explicit Plugin Path**: Users specify the plugin path explicitly using `--plugin <path>`.
4. **Single Plugin Mode**: Currently, the system loads one plugin at a time.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        naadcore-cli                             │
│  (CLI loader that loads plugins and processes MIDI events)     │
└───────────────────────┬─────────────────────────────────────────┘
                        │
                        │ MIDI events
                        ▼
┌─────────────────────────────────────────────────────────────────┐
│                      PluginManager                              │
│  (Singleton that manages plugin loading/unloading and routing) │
└───────────────────────┬─────────────────────────────────────────┘
                        │
                        ▼
              ┌──────────────┐
              │   Plugin     │
              │ (harmonium)  │
              └──────────────┘
```

## Plugin Interface

All plugins must implement the `INaadPlugin` interface defined in `include/naadcore/plugin.hpp`:

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

### Required C-Linkage Functions

Plugins must export the following C-linkage functions:

```cpp
extern "C" {
    naadcore::INaadPlugin* naad_plugin_create();
    void naad_plugin_destroy(naadcore::INaadPlugin* plugin);
    int naad_plugin_get_version();
}
```

## Well-known environment config keys

The host passes the user's audio environment to plugins through the config
seam (`set_config`) **BEFORE** `init()`. These keys are optional
capabilities, not ABI: `naad_plugin_get_version()` does NOT bump for
well-known keys, and no new virtual methods were added.

| Key | Meaning | Backing FluidSynth setting (harmonium) |
|---|---|---|
| `audio_driver` | Audio driver name (`alsa`, `pipewire`, `pulseaudio`, `file`, ...) | `audio.driver` |
| `audio_device` | Audio output device name (machine-specific: an ALSA PCM name such as `default` or `plughw:CARD=PCH,DEV=0`, or a PulseAudio sink name) | `audio.alsa.device` / `audio.pulseaudio.device` (mapped per driver at init) |

Rules:

- Plugins **SHOULD honor these keys or MAY return `PLUGIN_NOT_IMPLEMENTED`**
  from `set_config` — that is acceptable degradation, not an error (such a
  plugin simply runs on its own default). The host never fails a load over
  an unexpected `set_config` result; it only logs a warning.
- **The host applies both keys BEFORE `init()`** via `set_config` (the
  config seam). `audio_driver` is additionally the `init()` argument, which
  takes precedence over the stored `audio_driver` config value; the device
  deliberately has no `init()` parameter (no interface signature change) —
  the config seam is its only channel.
- **Validation happens at start, not at set time.** Device/driver names are
  machine-specific, so neither the host nor a well-behaved plugin rejects
  them at `set_config`; an unusable driver or device fails at
  `start_audio()` with `PLUGIN_ERROR` (e.g. `new_fluid_audio_driver`
  rejecting a bad ALSA PCM name), where the error is actionable.
- **Applies at start; restart to change.** A running instance's audio
  driver is not restarted (a driver restart would drone harmonium-style
  sustains), so a post-init `set_config("audio_device")` stores only — the
  CLI must be restarted to apply a new device/driver.
- For FluidSynth-based plugins the device string must be in the
  `fluid_settings` **before** `new_fluid_audio_driver` is called (i.e. at
  `init()`); see docs/PLUGIN_DEVELOPMENT.md "Environment keys every plugin
  SHOULD support" for the timing rule and multi-plugin caveats.

## Building a Plugin

### 1. Create the Plugin Header

```cpp
// plugins/my_plugin/my_plugin.hpp
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
};

#endif
```

### 2. Create the Plugin Implementation

```cpp
// plugins/my_plugin/my_plugin.cpp
#include "my_plugin.hpp"

// SoundFont path is embedded at compile time
#ifndef MY_PLUGIN_SOUNDFONT_PATH
#define MY_PLUGIN_SOUNDFONT_PATH "/path/to/soundfont.sf2"
#endif

namespace naadcore {

MyPlugin::MyPlugin() : /* ... */ {
}

MyPlugin::~MyPlugin() {
    stop_audio();
}

const PluginInfo* MyPlugin::get_info() {
    static PluginInfo info = {
        "my_plugin",
        "1.0.0",
        "Author Name",
        "Description of my plugin"
    };
    return &info;
}

PluginResult MyPlugin::init(const char* audio_driver) {
    // Initialize synthesizer
    // Load SoundFont from embedded path
    return PLUGIN_OK;
}

PluginResult MyPlugin::start_audio() {
    // Start audio driver
    return PLUGIN_OK;
}

PluginResult MyPlugin::stop_audio() {
    // Stop audio driver
    return PLUGIN_OK;
}

PluginResult MyPlugin::handle_midi_event(const MidiEvent& event) {
    // Process MIDI event
    return PLUGIN_OK;
}

std::string MyPlugin::get_config(const char* key) {
    return "";
}

PluginResult MyPlugin::set_config(const char* key, const char* value) {
    return PLUGIN_OK;
}

} // namespace naadcore

// Export C-linkage functions
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
}
```

### 3. Create CMakeLists.txt for the Plugin

```cmake
# plugins/my_plugin/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${FLUIDSYNTH_INCLUDE_DIRS})

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

set_target_properties(my_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    POSITION_INDEPENDENT_CODE ON
)

install(TARGETS my_plugin
    LIBRARY DESTINATION lib/naadcore/plugins
)
```

### 4. Register the Plugin in Main CMakeLists.txt

```cmake
# Add to main CMakeLists.txt
add_subdirectory(${CMAKE_SOURCE_DIR}/plugins/my_plugin)
```

## Using the CLI with Plugins

### Command Line Usage

```bash
# Basic usage with harmonium plugin (from the project root)
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0

# With audio driver override
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0 --audio-driver pipewire

# Show help
./build/apps/naadcore-cli/naadcore-cli --help
```

### SoundFont Requirements

The SoundFont path is **embedded at compile time** in each plugin. This means:

1. Each plugin must have a SoundFont compiled into it
2. No `--soundfont` CLI flag is available for plugins
3. The plugin binary is self-contained with its SoundFont

#### Setting the SoundFont Path

When building a plugin, specify the SoundFont path:

```bash
# For harmonium plugin (from the project root)
cmake -B build -DHARMONIUM_SOUNDFONT_PATH=/path/to/your/soundfont.sf2
cmake --build build -j4

# Or set it directly in the plugin's CMakeLists.txt (the actual mechanism;
# default = the committed in-repo font):
if(NOT HARMONIUM_SOUNDFONT_PATH)
    set(HARMONIUM_SOUNDFONT_PATH
        "${CMAKE_SOURCE_DIR}/plugins/harmonium/soundfonts/harmonium_v3.sf2")
endif()

target_compile_definitions(harmonium_plugin PRIVATE
    HARMONIUM_SOUNDFONT_PATH="${HARMONIUM_SOUNDFONT_PATH}"
)
```

#### Default SoundFont Path

The harmonium plugin is self-contained: its default font is committed in
the repository at `plugins/harmonium/soundfonts/harmonium_v3.sf2` (along
with the earlier derivation `harmonium_v2.sf2` and the upstream
provenance copy `harmonium_original.sf2` — see
`plugins/harmonium/soundfonts/README.md`). No font outside the repo is
needed; specify a custom path during build only for non-default fonts.

## Building the Harmonium Plugin

```bash
# Build the entire project (including plugins) — from the project root
cd /home/nil/Projects/Personal/naadcore
cmake -B build
cmake --build build -j4

# The harmonium plugin will be built to:
# /home/nil/Projects/Personal/naadcore/build/plugins/libharmonium_plugin.so
```

## MIDI Event Types

The plugin interface supports the following MIDI event types:

```cpp
enum Type {
    NOTE_ON = 1,
    NOTE_OFF = 2,
    CONTROL_CHANGE = 3,
    PITCH_BEND = 4,
    PROGRAM_CHANGE = 5,
    CHANNEL_PRESSURE = 6,
    KEY_PRESSURE = 7
};
```

## Creating a Custom SoundFont for a Plugin

The SoundFont is embedded at compile time using the `HARMONIUM_SOUNDFONT_PATH` preprocessor definition. To use a different SoundFont:

1. Place your SoundFont file in the plugin directory
2. Update the CMakeLists.txt with the correct path

```cmake
target_compile_definitions(my_plugin PRIVATE
    MY_PLUGIN_SOUNDFONT_PATH="${CMAKE_SOURCE_DIR}/custom.sf2"
)
```

## Architecture Files

- `include/naadcore/plugin.hpp` - Plugin interface definition
- `include/naadcore/plugin_manager.hpp` + `core/plugin_manager.cpp` - Plugin manager singleton
- `plugins/harmonium/` - Harmonium plugin implementation

## Testing

```bash
# Test that the plugin exports the required functions
nm -D ./build/plugins/libharmonium_plugin.so | grep naad_plugin

# Verify the plugin binary
file ./build/plugins/libharmonium_plugin.so
```

## Notes

- Plugins are loaded with `RTLD_NOW | RTLD_LOCAL` to ensure symbols are resolved immediately and not globally visible
- Each plugin manages its own audio driver and SoundFont
- The SoundFont path is a compile-time constant, not a runtime configuration
- The plugin system is designed for local, project-specific plugins, not system-wide distribution
- Instrument physics (e.g., the harmonium's uniform bellows velocity — keys pressed together sound at the first key's velocity, with the reference passing to the oldest still-held key when it is released, see `HANDOVER.md`) belongs inside plugins, not in the routing layer
