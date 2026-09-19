# NaadCore — Linux-Native Harmonium Synthesizer

NaadCore is a modular, plugin-based synthesizer framework for Linux. The harmonium
sound is delivered by the `harmonium` plugin (built on FluidSynth), loaded at runtime
by the `naadcore-cli` application.

```
Alesis Q49 (USB MIDI, ALSA 20:0)
        ↓
ALSA sequencer
        ↓
naadcore-cli (MIDI input, event routing)
        ↓
PluginManager → libharmonium_plugin.so (INaadPlugin)
        ↓
Embedded FluidSynth + harmonium.sf2
        ↓
ALSA audio → speakers
```

**Success criterion:** Press a key on the Q49 → hear the harmonium.

## Build

Requires: g++, CMake ≥ 3.16, FluidSynth ≥ 2.0, ALSA dev packages.

```bash
sudo apt install g++ cmake libfluidsynth-dev libasound2-dev
```

From the project root:

```bash
cd /home/nil/Projects/Personal/naadcore
cmake -B build
cmake --build build -j4
```

This builds exactly three targets:

| Target | Output |
|---|---|
| `naadcore_core` | `build/libnaadcore_core.so` (shared core library) |
| `harmonium_plugin` | `build/plugins/libharmonium_plugin.so` |
| `naadcore-cli` | `build/apps/naadcore-cli/naadcore-cli` |

## Run

### Interactive launcher (recommended)

```bash
./naadcore.sh
```

Walks you through everything: builds the CLI/plugins if needed (dependency
checks included), scans ALSA MIDI sources (`aconnect -o`) and lets you pick
one (with a rescan loop when the keyboard isn't plugged in yet), lists the
built plugins for you to choose, offers audio-driver selection and then an
audio output-device menu (driver-dependent: ALSA PCM names via `aplay`,
PipeWire/PulseAudio sinks via `pactl`, skipped for the pipewire driver —
`default` routes through PipeWire and pavucontrol/wpctl picks the
speaker/headphone port), warns if the SoundFont is missing, then launches
the CLI. Plain flags are still available:

### Direct CLI invocation

From the project root:

```bash
./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0
```

Expected output:

```
Loading plugin: ./build/plugins/libharmonium_plugin.so
Loaded SoundFont: .../plugins/harmonium/soundfonts/harmonium_v3.sf2 (ID: 1)
Loaded plugin: harmonium v1.0.0 (./build/plugins/libharmonium_plugin.so)
Plugin: harmonium v1.0.0
Starting audio...
Audio driver started: alsa
MIDI input connected: 20:0 -> 129:0
Ready. Press Ctrl+C to exit.
Listening for MIDI from 20:0
```

(FluidSynth may emit a few harmless warnings — see "Harmless warnings" below.)

Press Ctrl+C to exit. For scripted/foreground runs always wrap in `timeout`, e.g.:

```bash
timeout 5 ./build/apps/naadcore-cli/naadcore-cli --plugin ./build/plugins/libharmonium_plugin.so --midi 20:0
```

### CLI options

```
--plugin <path>       Path to plugin shared library (.so) (required)
--midi <client:port>  ALSA sequencer client:port for MIDI input (e.g. 20:0)
--audio-driver <name> Audio driver: alsa, pipewire, pulseaudio (default: alsa)
--audio-device <name> Audio output device passed to the plugin (unset =
                      plugin default). Applies at start — restart the CLI
                      to change it
--help                Show this help message
```

`--audio-device` is driver-specific:

- With the default `alsa` driver, an ALSA PCM name — e.g. `default`,
  `plughw:CARD=PCH,DEV=0`, or a hardware name from `aplay -L`. **Honest
  note for this laptop:** the default routes through PipeWire anyway
  (`pipewire-alsa` proxies the ALSA stream), so with `default` the
  destination port — speaker vs headphone jack — is chosen by PipeWire
  routing (pavucontrol / `wpctl`), not by this flag; a direct
  `hw:`/`plughw:` device bypasses PipeWire entirely (and breaks
  `parecord --monitor-stream` capture).
- With `pulseaudio`, a sink name from `pactl list short sinks`.
- The native `pipewire` driver has no device setting in this FluidSynth
  build — a device there is ignored with a warning.

There is no `--soundfont` flag: SoundFonts are embedded in plugins at build time.

### SoundFont embedding

The harmonium plugin is **self-contained**: every font it can load is
committed in the repository under `plugins/harmonium/soundfonts/`
(upstream provenance copy `harmonium_original.sf2` + derived fonts
`harmonium_v2.sf2` / `harmonium_v3.sf2` — see
`plugins/harmonium/soundfonts/README.md`). The path is compiled into the
plugin via `HARMONIUM_SOUNDFONT_PATH` in
`plugins/harmonium/CMakeLists.txt`. The default is the in-repo derived font
`plugins/harmonium/soundfonts/harmonium_v3.sf2` (preset 0 "harmonium" —
identical to the original font — preset 1 "harmonium double", the Phase 3
detuned 2-reed stop, and preset 2 "key click", the Phase 6 self-ending
chiff layer; see HANDOVER.md "Reed stops" and "Key click + micro-variation");
override it at configure time for a custom font:

```bash
cmake -B build -DHARMONIUM_SOUNDFONT_PATH=/path/to/other.sf2
cmake --build build -j4
```

## Verify the MIDI connection

The CLI registers with ALSA as client `naadcore` and subscribes to the Q49
automatically. In another terminal:

```bash
aconnect -l
```

You should see:

```
client 20: 'Q49' [type=kernel,card=1]
    0 'Q49 MIDI 1      '
client 129: 'naadcore' [type=user,pid=...]
    0 'naadcore input  '
        Connected From: 20:0
```

(If the subscription is missing, connect manually: `aconnect 20:0 129:0`.)

Play the Q49: single soft keys → quiet sound, single hard keys → loud sound,
chords → uniform bellows velocity (every key in the chord sounds at the first
key's velocity; when that key is released, the reference passes to the oldest
still-held key's original press velocity — see `HANDOVER.md`), release → sound
stops.

## Harmless warnings (safe to ignore)

- FluidSynth `SDL3`-related messages on startup
- GLib `g_param_spec` CRITICALs from FluidSynth
- `No preset found on channel 9` (channel 9 = GM percussion; unused here)
- `Failed to set thread to high priority`

## Troubleshooting

1. Kill stale instances: `pkill -f naadcore-cli`
2. **Continuous drone / stuck note**: harmonium samples sustain forever (no
   natural decay), so a lost NoteOff plays endlessly. Fix: press the droning
   key once more, or send All Notes Off (also resets the bellows state):
   `aseqsend -p 129:0 "B0 7B 00"` (use the actual port from `aconnect -l`)
3. Try another audio driver: `--audio-driver pulseaudio` (the native `pipewire`
   driver fails on this machine's FluidSynth build — the default `alsa` and
   `pulseaudio` drivers are both proxied by PipeWire anyway)
4. Check the subscription: `aconnect -l` must show `Connected From: 20:0` on the
   `naadcore input` port
5. Verify the Q49 is sending: `aseqdump -p 20:0` while pressing keys
6. Send a note by hand (this system's `aseqsend` uses positional hex syntax):
   `aseqsend -p 129:0 "90 60 100"` (note on) and `aseqsend -p 129:0 "80 60 0"` (note off)

## Project structure

```
naadcore/
├── CMakeLists.txt                  # Root build: naadcore_core, naadcore-cli, harmonium_plugin
├── include/naadcore/               # Public headers (canonical)
│   ├── plugin.hpp                  # INaadPlugin interface, MidiEvent, PluginInfo, PluginResult
│   ├── plugin_manager.hpp          # PluginManager singleton
│   └── midi.hpp                    # MidiInput + Synthesizer declarations
├── core/                           # Core library sources
│   ├── midi.cpp                    # MidiInput (ALSA) + Synthesizer (FluidSynth wrapper)
│   └── plugin_manager.cpp          # PluginManager implementation (dlopen/dlsym)
├── apps/naadcore-cli/              # CLI application
│   ├── CMakeLists.txt
│   └── main.cpp                    # Plugin loader + MIDI routing main loop
├── plugins/harmonium/              # Harmonium plugin (reference implementation)
│   ├── CMakeLists.txt              # Embeds HARMONIUM_SOUNDFONT_PATH
│   ├── soundfonts/                 # Committed fonts + provenance README
│   │                               #   (harmonium_original/_v2/_v3.sf2)
│   ├── harmonium_plugin.hpp
│   └── harmonium_plugin.cpp        # Embedded FluidSynth synth, exports C factory functions
├── tests/                          # Realism test harness (T1–T7 tracks, render/capture
│   │                               #   scripts, analyze.py, config-seam tests)
│   └── README.md
└── docs/                           # Architecture and plugin-system documentation
```

## Documentation

- `HANDOVER.md` — authoritative handover / current state
- `docs/NAADCORE_ARCHITECTURE.md` — system architecture
- `docs/PLUGIN_SYSTEM.md` — plugin system overview
- `docs/PLUGIN_DEVELOPMENT.md` — how to write a new plugin
- `docs/PLUGIN_SYSTEM_IMPLEMENTATION.md` — plugin system implementation notes
- `docs/HARMONIUM_CONFIG.md` — harmonium plugin config-key registry
  (voicing, envelope, stops, coupler, drone, key-click, variation)
- `docs/HARMONIUM_SF2_AUDIT.md` — harmonium.sf2 structure audit
- `docs/NAADCORE_MVP_CHALLENGE.md` — historical MVP record (completed)
- `tests/README.md` — realism test harness (test tracks, renders, A/B workflow)

## License

MIT License (to be determined)