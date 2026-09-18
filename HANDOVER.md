# NaadCore Handover — Authoritative State Document

Last updated: 2026-09-19 (harmonium realism Phase 3: reed stops — `stop`
config key (single/double) selecting SoundFont presets in the new in-repo
derived font plugins/harmonium/soundfonts/harmonium_v2.sf2 (+4¢ detuned
2-reed layering = the signature slow beating); incoming PROGRAM_CHANGE
events now IGNORED (stops are config-controlled); CMake default
HARMONIUM_SOUNDFONT_PATH repointed at the in-repo font (override still
available); PROGRAM_CHANGE ignore verified objectively; 96/96 config tests;
see "Reed stops (Phase 3)" below. Phases 0–2 unchanged: synth voicing
pinned in plugin — gain/reverb/chorus defaults + live config keys; runtime
envelope shaping — attack_ms/release_ms live config keys via FluidSynth
channel generators; plugin-in-loop offline renderer (now with KEY=VALUE
config overrides); test harness under tests/; SF2 audited — see
docs/HARMONIUM_SF2_AUDIT.md; `--audio-driver` CLI flag wired through;
interactive launcher naadcore.sh added)

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
  - SoundFont path compiled in (embedded at build time; default is the
    in-repo derived font, see "Reed stops (Phase 3)")
        ↓
FluidSynth + plugins/harmonium/soundfonts/harmonium_v2.sf2 → ALSA audio → speakers
```

The CLI registers with ALSA under the client name **`naadcore`** and creates a
write-capable input port ("naadcore input") that subscribes to the Q49 port at
startup — no manual `aconnect` needed.

## Complete post-cleanup file map

```
naadcore/
├── CMakeLists.txt                  # Root build — exactly 3 targets (see below)
├── naadcore.sh                     # Interactive launcher: build → MIDI scan/menu
│                                   #   → plugin menu → driver menu → run CLI
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
│       ├── CMakeLists.txt          # Embeds HARMONIUM_SOUNDFONT_PATH (default:
│       │                           #   in-repo harmonium_v2.sf2), outputs to build/plugins/
│       ├── soundfonts/
│       │   └── harmonium_v2.sf2    # Derived font (Phase 3, committed): preset 0
│       │                           #   "harmonium" (byte-identical to the original)
│       │                           #   + preset 1 "harmonium double" (+4¢ zones)
│       ├── harmonium_plugin.hpp
│       └── harmonium_plugin.cpp     # FluidSynth plugin + extern "C" factories
├── tests/                          # Realism test harness (re-added with content)
│   ├── README.md                   # Harness guide, tool status, capture paths
│   ├── RESULTS.md                  # A/B score sheet + objective measurements
│   ├── analyze.py                  # WAV analysis (onset/release/AM/peak/RMS)
│   ├── test_plugin_config.cpp      # Config-seam unit tests (96 checks)
│   ├── midi/                       # 7 base test tracks (T1–T7) + T8/T9
│   │                               #   Phase 3 probe tracks
│   ├── scripts/                    # gen_midi.py, render_sf2.sh, capture_live.sh,
│   │                               #   render_plugin.cpp, run_render_plugin.sh,
│   │                               #   derive_sf2.py (Phase 3 SF2 surgery),
│   │                               #   am_spectrum.py (AM-band spectrum), ...
│   ├── timings/                    # Note timing files used by analyze.py
│   ├── renders/                    # Rendered/captured WAVs (gitignored)
│   └── references/                 # Reference clips (gitignored, personal use)
└── docs/
    ├── NAADCORE_ARCHITECTURE.md    # Architecture overview (current)
    ├── PLUGIN_SYSTEM.md            # Plugin system design/usage
    ├── PLUGIN_SYSTEM_IMPLEMENTATION.md # Implementation notes (current)
    ├── PLUGIN_DEVELOPMENT.md       # Guide for writing new plugins
    ├── HARMONIUM_SF2_AUDIT.md      # harmonium.sf2 structure audit (Phase 1)
    ├── NAADCORE_MVP_CHALLENGE.md   # HISTORICAL: MVP design record (completed)
    ├── CODEBASE_ANALYSIS.md        # Analysis of the separate harmonium-companion web project
    └── README.md                   # Technical notes / API reference
```

Removed in an earlier cleanup (do NOT recreate): `apps/harmonium.cpp`,
`core/harmonium.hpp`, `include/naadcore/harmonium.hpp`, `core/midi.hpp`
(duplicate of `include/naadcore/midi.hpp`), `core/plugin_manager.hpp`
(duplicate of `include/naadcore/plugin_manager.hpp`),
and the `naadcore-harmonium` MVP executable target. The empty `tests/`
directory removed then has since been **re-added with harness content**
(2026-09-18) — it is now a permanent part of the tree.

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
Synth voicing: gain=0.4 reverb=on chorus=off interp=4th-order
Synth envelope: attack_ms=10 release_ms=200
Loaded SoundFont: /home/nil/Projects/Personal/naadcore/plugins/harmonium/soundfonts/harmonium_v2.sf2 (ID: 1)
Synth stop: single
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
`get_loaded_plugins()`, `set_audio_driver(driver)` (driver passed to plugins
at `init()`; empty/nullptr = plugin's own default), `initialize(audio_driver)`,
`start_all_audio()`, `stop_all_audio()`, `cleanup()`. Thread-safe (recursive
mutex).

The CLI parses `--audio-driver <name>` (alsa/pipewire/pulseaudio, default
alsa) and passes it to the plugin via `PluginManager::set_audio_driver()`
before loading. Verified: `--audio-driver pulseaudio` starts FluidSynth's
PulseAudio driver; the native `pipewire` driver fails on this machine's
FluidSynth 2.4.8 build (missing `pw_init()`, independent of NaadCore) —
the default `alsa` and `pulseaudio` drivers are both proxied by PipeWire
anyway.

## Embedded SoundFont mechanism

The harmonium plugin has no runtime SoundFont flag. The path is baked in at
compile time:

- `plugins/harmonium/CMakeLists.txt` sets the CMake variable
  `HARMONIUM_SOUNDFONT_PATH` (default since Phase 3:
  `${CMAKE_SOURCE_DIR}/plugins/harmonium/soundfonts/harmonium_v2.sf2` —
  the in-repo derived font, portable across machines; the original
  machine-specific `/home/nil/harmonium-companion/harmonium.sf2` remains
  available via `-D` override)
  and passes it as a `target_compile_definitions(... PRIVATE)` preprocessor macro.
- `harmonium_plugin.cpp` has a `#ifndef HARMONIUM_SOUNDFONT_PATH` fallback with
  the same path, then loads it via `fluid_synth_sfload()` during `init()`.
- Override for custom builds: `cmake -B build -DHARMONIUM_SOUNDFONT_PATH=/path/to.sf2`
  (or at runtime via the `soundfont_path` config key before `init()`).

## Uniform Bellows Velocity (harmonium plugin)

On a real harmonium the bellows drive every open reed at the same air pressure,
so a chord sounds uniform no matter how unevenly the fingers press. The
harmonium plugin models this entirely inside `handle_midi_event()`
(`plugins/harmonium/harmonium_plugin.cpp`); the CLI, PluginManager, and the
shared `MidiEvent`/`INaadPlugin` headers are untouched.

Semantics:

- **Reference:** the first NoteOn of a key sequence (no keys currently held)
  latches its velocity as the bellows **reference velocity** and plays at its
  own velocity. Every NoteOn arriving while any key is still held plays at the
  reference velocity instead.
- **Baton-pass on release:** each held key remembers its ORIGINAL press
  velocity in press order. When the reference (oldest held) key is released
  while other keys are still held, the reference passes to the next oldest
  held key's original press velocity. Example: press n@100, press m@20 (m
  sounds at 100), release n, press j@60 → j sounds at 20 (m's original).
  A full release (all keys up) resets the reference; the next NoteOn starts a
  fresh sequence.
- **Legato inherits:** a melodic NoteOn while another key is held also plays
  at the reference velocity (physically faithful; dynamics flatten until all
  keys are released — accepted trade-off).
- **No retroactive re-velocity:** already-sounding notes are never re-triggered.
- **Global bellows:** held-note state ignores the MIDI channel (one harmonium,
  one bellows); a NoteOn with velocity 0 still counts as a release, and a
  duplicate NoteOn for an already-held key plays at the reference velocity
  without changing its recorded original velocity or position.
- **CC 123 (All Notes Off)** clears the sequence state in addition to the
  normal FluidSynth passthrough (stuck-note insurance).

State: `std::vector<HeldNote> held_notes_` (`{note, original velocity}`,
front = oldest pressed) + `uint8_t reference_velocity_` in `HarmoniumPlugin`
(see `harmonium_plugin.hpp`).

## Synth voicing (Phase 1, 2026-09-18)

Pinned in `HarmoniumPlugin::init()` (replaces FluidSynth library defaults,
which drift across versions):

- `synth.gain` = 0.4 (`fluid_synth_set_gain`)
- Reverb ON, "small room": roomsize 0.2, damp 0.0, width 0.3, level 0.4
  (FluidSynth 2.4 group API, `fx_group=-1` = all groups)
- Chorus OFF (reeds have no chorus; shimmer comes from in-sample beating)
- 4th-order interpolation on all channels
- SoundFont ID from `fluid_synth_sfload` stored as `soundfont_id_` (needed
  for future preset/stop selection)

Live config keys (via `set_config`/`get_config`, no CLI surface yet):

| Key | Values | Effect |
|---|---|---|
| `gain` | float 0–10 | `fluid_synth_set_gain`, strict validation |
| `reverb` | on/off | `fluid_synth_reverb_on` all groups |
| `chorus` | on/off | `fluid_synth_chorus_on` all groups |
| `attack_ms` | int 1–2000 | vol-env attack via `GEN_VOLENVATTACK` (Phase 2) |
| `release_ms` | int 1–4000 | vol-env release via `GEN_VOLENVRELEASE` (Phase 2) |
| `stop` | single/double | reed stop → SoundFont preset via `program_select` (Phase 3) |

Plus the pre-existing keys: `soundfont_path`, `audio_driver`.

Verified: 96/96 config-seam checks pass (`tests/scripts/run_config_tests.sh`);
live capture peak level matches the offline render exactly (−25.7 dBFS for
note 60 @ vel 100).

## Volume-envelope shaping (Phase 2, 2026-09-18)

The SF2 has a clicky ~1 ms attack and a 100 ms release (see
docs/HARMONIUM_SF2_AUDIT.md). Phase 2 shapes both **at runtime via FluidSynth
channel generators** (`fluid_synth_set_gen` with `GEN_VOLENVATTACK` /
`GEN_VOLENVRELEASE` on all 16 MIDI channels) — the SF2 file is untouched.

- Defaults: `attack_ms_` = 10 (softened reed speech), `release_ms_` = 200
  (breathier bellows tail). Sustain is left at the font's full level
  (0 cB attenuation) — organ-like, correct for harmonium.
- Applied in `init()` (before `sfload` — verified the generators survive
  font loading) and on every live `attack_ms`/`release_ms` set_config change
  (`apply_envelope_gens()`). Second log line: `Synth envelope:
  attack_ms=… release_ms=…`.
- Top octave (keys 65–84) is one F4 sample stretched up to +19 semitones —
  NOT addressable by envelope work; needs new samples (Phase 7, open).

**set_gen override-vs-additive finding (calibrated empirically, 2026-09-18,
FluidSynth 2.4.8, raw-FluidSynth file-driver renders measured with
tests/analyze.py):**

- `fluid_synth_set_gen` values are **ADDITIVE OFFSETS** on top of the
  instrument zone's generator values, NOT overrides: release gen 0 left the
  font's 100 ms release at 99 ms (override would give 1000 ms); +1200 gave
  203 ms measured (override would give 2 s); −3986 gave 17 ms (override
  would give 100 ms). Same for the attack gen.
- Offsets are **not clamped** to the SF2 spec range (±12000): an attack
  offset of +13200 produced the full 2 s nominal attack (a ±12000 clamp
  would have produced 1 s).
- There is no `fluid_synth_set_gen2` in 2.4.8 — only
  `fluid_synth_set_gen`/`fluid_synth_get_gen`. Generator enum names are
  `GEN_VOLENVATTACK`/`GEN_VOLENVRELEASE` (gen.h), not `FLUID_GEN_*`.
- Compensation: the plugin converts the requested absolute time to the
  offset that moves the font's own base value to it:
  `offset_tc = 1200·log2(ms/1000) − base_tc` with the font bases
  `kSf2AttackTc = −12000` (~1 ms) and `kSf2ReleaseTc = −3986` (100 ms)
  from the audit. Sustain is not touched.

Measured effect (live captures, same analysis pipeline both sides —
tests/RESULTS.md has the full table): T1 release-to-−60 dB 46–70 ms →
81–122 ms; T4 staccato 80/80 onsets still distinct, tail at the next onset
≈44 dB below the note peak (no smear).

## Reed stops / 2-reed detuned layering (Phase 3, 2026-09-19)

The signature harmonium "slow beating/shimmer": two slightly detuned unison
reeds per note beat against each other at the difference frequency. Phase 3
implements a `stop` config key backed by SoundFont presets in a **derived
font** — the original `/home/nil/harmonium-companion/harmonium.sf2` was
never modified.

**Derived font** `plugins/harmonium/soundfonts/harmonium_v2.sf2` (committed,
generated by `tests/scripts/derive_sf2.py`, regenerable):

| Preset | Name | Instrument | Sound |
|---|---|---|---|
| bank 0 / prog 0 | harmonium | 0 (byte-identical copy) | today's single-reed sound |
| bank 0 / prog 1 | harmonium double | 1 (every key zone duplicated with `fineTune = +4¢`) | two detuned unison reeds per note |

Surgery details: preset zones preserved byte-identically for preset 0; the
duplicate zones copy the zone's full generator list (keyRange, attenuation,
sample refs, loop offsets) and only add fineTune (gen 52, spec range ±99¢).
Bag/gen indices renumbered, EOI/EOP terminals kept, sizes recomputed; the
6.6 MB sample data is referenced, not duplicated (+476 bytes total).
Validation: `sf2_audit.py` shows 2 presets/2 instruments/doubled zones;
FluidSynth loads it; preset 0 renders **sample-exact** vs the original font.

**Plugin `stop` key** (`set_config`/`get_config`, strict validation like the
other keys):

| Value | Preset | Meaning |
|---|---|---|
| `single` (default) | 0 | single reed — today's sound |
| `double` | 1 | 2 detuned unison reeds per note (slow beating 0.2–2 Hz) |

- Applied via `fluid_synth_program_select(synth_, ch, soundfont_id_, 0, preset)`
  on ALL MIDI channels — live when the synth is ready, otherwise stored and
  applied in `init()` right after `sfload` (which first selects preset 0
  explicitly on all channels, then the stored stop — deterministic regardless
  of FluidSynth's reset behavior). Log line: `Synth stop: <name>`.
- **PROGRAM_CHANGE policy change:** incoming PROGRAM_CHANGE events are now
  **IGNORED** (`handle_midi_event` swallows them, PLUGIN_OK). Rationale:
  stops are config-controlled; forwarding program changes to FluidSynth
  would silently switch reed stops (a stray program change would wreck the
  voicing). Verified objectively (T8 probe: a program change to program 1
  in the MIDI file leaves the stop=single render single-reed). **Future
  idea:** program changes could become MIDI-mapped stop switches.
- Uniform bellows velocity + envelope shaping are untouched.

**Beat-rate expectations** (beat = f·(2^(D/1200)−1), D = 4¢; measured on the
T9 probe via carrier-line FFT, plugin-in-loop renders):

| Note | Font pitch | Expected beat | Measured beat |
|---|---|---|---|
| 43 | 103.33 Hz | 0.24 Hz | < 0.19 Hz (below the 6 s analysis window's resolution; comb visible in the envelope spectrum) |
| 60 | 277.04 Hz | 0.64 Hz | 0.56 Hz |
| 79 | 832.22 Hz | 1.92 Hz | 1.85 Hz |

Pitch is stable (mean +1.5¢ by design; detune is 0¢/+4¢, not symmetric).
Objective numbers: tests/RESULTS.md Phase 3. `four` (2 unison + octave
pair) was **not** implemented — the core is solid but the octave coupler
(Phase 4) supersedes the octave-pair half; revisit there.

**Renderer support:** `run_render_plugin.sh` now accepts trailing
`KEY=VALUE` config pairs applied via `set_config` before `init()` —
e.g. `run_render_plugin.sh T1.mid out.wav 3 stop=double`. `render_sf2.sh`
defaults to the in-repo derived font (`HARMONIUM_SOUNDFONT` env overrides).

## Harmonium realism test harness (Phase 0, 2026-09-18)

`tests/` (see `tests/README.md` for full details):

- 7 test MIDI tracks (T1–T7): envelope, legato, chords, staccato, drone,
  repertoire phrase, velocity sweep — channel 0, no CCs/program changes
- `render_sf2.sh` — offline FluidSynth render (baseline/phase-voicing modes).
  NOTE: cannot reflect plugin behavior (the CLI can't set generators) — use
  the plugin-in-loop renderer below for that
- `run_render_plugin.sh` + `render_plugin.cpp` — offline render with the
  actual plugin .so IN the loop (dlopen + FluidSynth "file" audio driver,
  MIDI file events fed at file timing). Reflects voicing, envelope
  generators, and bellows velocity. Real-time (~1.15× track length).
- `capture_live.sh` — live capture through CLI → plugin → audio. On this
  machine the PipeWire path works: the plugin's ALSA output is proxied by
  `pipewire-alsa`, the stream is captured with `parecord --monitor-stream`
- `analyze.py` — onset/release times, sustained-note AM rate, peak/RMS
- SF2 audit findings in `docs/HARMONIUM_SF2_AUDIT.md` — headline: top
  octave (keys 65–84) is ONE sample stretched up to +19 semitones; attack
  ≈1 ms, release 100 ms, no modulators, no LFO, ~3 Hz beating recorded into
  loops

## Harmless warnings (do not chase these)

- FluidSynth SDL3-related startup messages
- GLib `g_param_spec` CRITICALs emitted by FluidSynth
- `No preset found on channel 9` (GM percussion channel, unused)
- `Failed to set thread to high priority`

## Stuck notes / continuous drone (escape hatch)

Harmonium samples sustain indefinitely (organ-type reeds, no natural decay),
so **any NoteOn without a matching NoteOff drones forever** — unlike piano
SoundFonts, stuck notes never self-heal. Causes seen so far: lost NoteOff over
USB, notes left held when a test/automation script kills the CLI, or a stale
background CLI process still holding notes (check with
`ps aux | grep naadcore-cli`, kill with `pkill -f naadcore-cli`).

To clear a stuck note in a live instance:
- press the droning key once more (NoteOn + NoteOff), or
- send All Notes Off — the plugin also resets its bellows state on it:
  `aseqsend -p <naadcore port> "B0 7B 00"` (read the port from `aconnect -l`;
  client numbers are dynamic), or
- restart the CLI.

## Known minor issues

- The CLI loads exactly one plugin per run (PluginManager supports multiple via
  `route_midi_event` fan-out, but the CLI only takes one `--plugin` argument).
- ALSA client numbers for the CLI (e.g. 129) are dynamic; always read the actual
  number from `aconnect -l` rather than assuming.
- `MidiEvent::timestamp` is never filled in by the CLI conversion layer (stays 0).
- The `Synthesizer` class inside `core/midi.cpp` is currently unused by the plugin
  (plugins embed their own FluidSynth instance); it remains as core-library API.
- No CC/passport handling beyond what FluidSynth does natively; no raga,
  coupler, or GUI features yet. (Uniform bellows velocity IS implemented —
  see "Uniform Bellows Velocity" above; bellows pressure/expression modeling
  such as CC#11 is still future work.)

## Suggested next steps

1. **Harmonium realism Phases 4+** (active effort — Phases 0–3 complete):
   - Phase 4: octave coupler / sub-octave layer router (supersedes the
     unimplemented `four` stop's octave pair); duplicate-NoteOn ignore;
     CC 123 across all 16 channels
   - Phase 5: drone (unpika) + config-key registry doc
   - Phase 7 (envelope work can't fix this): high-register stretch — keys
     65–84 are one F4 sample stretched up to +19 semitones; needs new samples
   - Optional Phase 3 polish: `four` stop (2 unison + octave pair) in the
     derived font if the coupler doesn't cover it; raise D at the low end
     (note 43's beat 0.24 Hz is at the slow edge); live `stop` switching
     via capture_live.sh (needs a config plumb)
    - Reference clips still pending (yt-dlp/sox not installable non-interactively)
2. **Multiple plugin support in CLI**: accept several `--plugin` flags or a
   plugin directory; route MIDI to all loaded plugins (PluginManager already
   fans out).
3. **Plugin configuration surface**: wire `set_config/get_config` to CLI
   flags, a config file, or MIDI CC mappings (the realism plan recommends
   in-plugin CC mappings to avoid CLI changes).
4. **Raga selection**: implement raga note filtering in the harmonium plugin
   (see docs/CODEBASE_ANALYSIS.md for the harmonium-companion raga/sargam logic
   worth porting).
5. **Bellows/expression modeling**: map a MIDI controller (CC#11 or velocity
   envelope) to harmonium air-pressure expression (deliberately deferred —
   air is assumed 100% for the current realism phases).
6. **Additional plugins**: pipe organ or drone/tanpura plugin using the same
   INaadPlugin contract as a portability proof.
7. **Packaging**: install rules exist (`bin`, `lib/naadcore`, `lib/naadcore/plugins`);
   consider CPack or a proper install layout.