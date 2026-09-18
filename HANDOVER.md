# NaadCore Handover — Authoritative State Document

Last updated: 2026-09-19 (harmonium realism Phase 6: key-click/chiff layer +
per-note micro-variation — the polish phase. Derived font
plugins/harmonium/soundfonts/harmonium_v3.sf2 (now the CMake default) adds a
synthesized 40 ms key-noise "KeyClick" sample and preset 2 "key click" — a
SELF-ENDING click instrument (attack 1 ms / decay 40 ms / sustain fully
closed; the voice dies ≤21 ms after onset, proven by render — a
self-sustaining click would be a permanent drone); preset 0 is byte-identical
to v2 (T1 render compared). The plugin triggers the click on internal channel
12 on every ACCEPTED main-path NoteOn (never for drone changes, never for a
swallowed duplicate NoteOn — no pallet moved, no click), controlled by the
`key_click` config key ("off" default / "low"=vel 45 / "high"=vel 75, CC 7=64
on ch12); micro-variation via the `variation` key ("on" default / "off"):
a fixed-seed mt19937 (seed 20260919) jitters only the velocity handed to
FluidSynth (±1..3 main, ±4..8 click, anti-repeat so consecutive notes never
coincide) — the bellows reference/baton bookkeeping stays exact, off is
byte-comparable against Phase 5; variation=on defeats sample-identical
repeats (measured ±0.1–0.6 dB per note, adjacent repeats never identical,
deterministic across runs to 0.000 dB). Channel reservation is now
15 coupler / 14 sub-octave / 13 drone / 12 click. 267/267 config tests;
authoritative config-key registry at docs/HARMONIUM_CONFIG.md; see
"Key click + micro-variation (Phase 6)" below. Phases 0–5 unchanged: synth
voicing pinned in plugin — gain/reverb/chorus defaults + live config keys;
runtime envelope shaping — attack_ms/release_ms live config keys via
FluidSynth channel generators; reed stops — `stop` config key (single/double)
selecting presets in the in-repo derived fonts, PROGRAM_CHANGE events
ignored; layer router — octave coupler on internal channel 15 (note+12,
~6–8 dB below main, +3¢ detuned) and sub-octave on channel 14 (note−12,
~11–14 dB below main), `coupler` / `sub_octave` live config keys with
mid-phrase toggling while notes are held; drone (unpika) on internal channel
13 — config-controlled fixture notes (`drone` key: "off" or up to 8
comma-separated MIDI note numbers) sounding continuously under the melody,
never touching the bellows model, fixed velocity 100 with loudness from
`drone_level` (CC 7 on ch13, default 45, measured ≈11–14 dB under the melody
fundamental), live add/remove semantics, not cleared-and-forgotten by CC 123,
no pitch-bend/CC11 mirroring; pitch bend + CC 11 mirrored to coupler/sub only,
CC 7 deliberately NOT mirrored (it IS the layer gain); duplicate NoteOn on a
held key IGNORED (no re-attack); CC 123 hardened to all_notes_off on ALL 16
channels plus drone reset; cross-channel NoteOff releases voices on the
note's original channel; uniform bellows velocity model (held_notes_ vector,
reference_velocity_ latch/baton-pass); plugin-in-loop offline renderer with
KEY=VALUE config overrides; test harness under tests/; SF2 audited — see
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
FluidSynth + plugins/harmonium/soundfonts/harmonium_v3.sf2 → ALSA audio → speakers
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
│       │                           #   in-repo harmonium_v3.sf2), outputs to build/plugins/
│       ├── soundfonts/
│       │   ├── harmonium_v3.sf2    # Derived font (Phase 6, committed, DEFAULT):
│       │   │                       #   presets 0 "harmonium" (byte-identical to
│       │   │                       #   the original) + 1 "harmonium double"
│       │   │                       #   (+4¢ zones) + 2 "key click" (self-ending
│       │   │                       #   chiff instrument + synthesized sample)
│       │   └── harmonium_v2.sf2    # Derived font (Phase 3, committed, kept for
│       │                           #   comparability): presets 0/1 only
│       ├── harmonium_plugin.hpp
│       └── harmonium_plugin.cpp     # FluidSynth plugin + extern "C" factories
├── tests/                          # Realism test harness (re-added with content)
│   ├── README.md                   # Harness guide, tool status, capture paths
│   ├── RESULTS.md                  # A/B score sheet + objective measurements
│   ├── analyze.py                  # WAV analysis (onset/release/AM/peak/RMS)
│   ├── test_plugin_config.cpp      # Config-seam unit tests (213 checks)
│   ├── midi/                       # 7 base test tracks (T1–T7) + T8–T13
│   │                               #   Phase 3/4/5 probe tracks
│   ├── scripts/                    # gen_midi.py, render_sf2.sh, capture_live.sh,
│   │                               #   render_plugin.cpp, run_render_plugin.sh,
│   │                               #   derive_sf2.py (Phase 3/6 SF2 surgery:
│   │                               #   --click builds harmonium_v3.sf2),
│   │                               #   am_spectrum.py (AM-band spectrum),
│   │                               #   gen_probes_phase4.py (T10/T11),
│   │                               #   gen_probes_phase5.py (T12/T13), ...
│   ├── timings/                    # Note timing files used by analyze.py
│   ├── renders/                    # Rendered/captured WAVs (gitignored)
│   └── references/                 # Reference clips (gitignored, personal use)
└── docs/
    ├── NAADCORE_ARCHITECTURE.md    # Architecture overview (current)
    ├── PLUGIN_SYSTEM.md            # Plugin system design/usage
    ├── PLUGIN_SYSTEM_IMPLEMENTATION.md # Implementation notes (current)
    ├── PLUGIN_DEVELOPMENT.md       # Guide for writing new plugins
    ├── HARMONIUM_SF2_AUDIT.md      # harmonium.sf2 structure audit (Phase 1)
    ├── HARMONIUM_CONFIG.md         # Authoritative config-key registry
    │                               #   (all set_config/get_config keys, Phase 5)
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
Loaded SoundFont: /home/nil/Projects/Personal/naadcore/plugins/harmonium/soundfonts/harmonium_v3.sf2 (ID: 1)
Synth stop: single
Synth layers: coupler=off sub_octave=off (ch15=note+12 CC7=60, ch14=note-12 CC7=40)
Synth click: key_click=off variation=on (ch12 preset 2 CC7=64, vel low/high=45/75, jitter main +-1..3 click +-1..8, seed 20260919)
Synth drone: off (ch13 CC7=45 vel=100)
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
  `HARMONIUM_SOUNDFONT_PATH` (default since Phase 6:
  `${CMAKE_SOURCE_DIR}/plugins/harmonium/soundfonts/harmonium_v3.sf2` —
  the in-repo derived font with the click preset, portable across
  machines; the Phase 3 font harmonium_v2.sf2 stays committed for
  comparability; the original machine-specific
  `/home/nil/harmonium-companion/harmonium.sf2` remains available via
  `-D` override)
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
- **No retroactive re-velocity:** already-sounding notes are never re-triggered
  or re-velocityed.
- **Global bellows:** held-note state ignores the MIDI channel (one harmonium,
  one bellows); a NoteOn with velocity 0 still counts as a release, and a
  **duplicate NoteOn for an already-held key is IGNORED** (Phase 4 policy
  change: the pallet is already open — re-triggering re-attacked the reed
  mid-phrase; now nothing is updated and no FluidSynth call is made. This is
  an audible change from Phase ≤3, where the duplicate re-noteoned at the
  reference velocity).
- **Cross-channel release (Phase 4):** a NoteOff for a held note releases its
  voices on the note's ORIGINAL channel(s) no matter which channel the off
  arrived on (the channel is stored per held note; bellows bookkeeping stays
  channel-agnostic).
- **CC 123 (All Notes Off)** clears the sequence state AND calls
  `fluid_synth_all_notes_off` on ALL 16 channels (a one-channel CC 123 only
  cleared that channel — layer voices on internal channels 14/15 would have
  been stranded). Since Phase 5 it also clears the drone's sounding-note
  container (see "Drone (Phase 5)").

State: `std::vector<HeldNote> held_notes_` (`{note, original press velocity,
channel, sounding_velocity, layers}`, front = oldest pressed) +
`uint8_t reference_velocity_` in `HarmoniumPlugin` (see
`harmonium_plugin.hpp`). `sounding_velocity` = the velocity actually played
(the reference at press time) — needed so mid-phrase layer toggles start
the right voices; `layers` = bit flags of the internal-channel voices
currently sounding for the note.

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
| `coupler` | on/off | octave coupler layer (Phase 4) |
| `sub_octave` | on/off | sub-octave layer (Phase 4) |
| `drone` | off / note list | drone fixture on channel 13 (Phase 5) |
| `drone_level` | int 0–127 | drone gain, CC 7 on channel 13 (Phase 5) |
| `key_click` | off/low/high | key-click/chiff layer on channel 12 (Phase 6) |
| `variation` | on/off | per-note velocity micro-variation (Phase 6) |

Plus the pre-existing keys: `soundfont_path`, `audio_driver`.

**The authoritative per-key contract** (type/format, defaults, live-vs-init
semantics, echo behavior, invalid-input behavior, FluidSynth mechanism) now
lives in **docs/HARMONIUM_CONFIG.md** — consult that registry first.

Verified: 96/96 config-seam checks at the time (now 267/267 — see
tests/RESULTS.md; `tests/scripts/run_config_tests.sh`); live capture peak
level matches the offline render exactly (−25.7 dBFS for note 60 @ vel 100).

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

## Layer router — octave coupler + sub-octave (Phase 4, 2026-09-19)

The plugin routes each held note to up to three FluidSynth voices: the main
voice on the incoming channel, plus optional fixed internal layers, all on
the current `stop` preset. Channels 12/13/14/15 are RESERVED for the router,
drone and click layer — MIDI input arriving on them from a controller will
collide with layer/drone/click voices (the Q49 sends on one channel only;
document any multi-channel controller use).

| Layer | Internal channel | Pitch | Gain (measured vs main voice) | Config key |
|---|---|---|---|---|
| main | incoming channel (0–12 safe) | note | 0 dB | — |
| octave coupler | **15** | note+12 | **−6.4…−7.8 dB** (CC7=60) | `coupler` (default off) |
| sub-octave | **14** | note−12 | **−11.4…−13.9 dB** (CC7=40; −22 dB measured at note 79 — its sub sample sits lower) | `sub_octave` (default off) |
| drone (Phase 5) | **13** | fixed spec | **−11…−14 dB** under the melody (CC7=45) | `drone` / `drone_level` |

- **All layer voices sound at `reference_velocity_`** (the bellows reference
  at press time) — the uniform bellows velocity model is untouched: one
  bellows, layers never fork it, no retroactive re-velocity.
- **Range clamps:** note+12 > 127 (coupler) or note−12 < 0 (sub) → that
  layer's voice is silently skipped for the note. The font's lowest zone
  covers keys 0–43 (G2 stretched), so sub-octave voices exist down to
  note 0 (heavily stretched below ~36 — thin but present).
- **HeldNote** carries `{note, velocity, channel, sounding_velocity,
  layers}`: `channel` = incoming channel (all main-voice noteoffs use it, so
  a cross-channel NoteOff cannot strand a voice); `sounding_velocity` = the
  velocity actually played (needed for mid-phrase layer toggles); `layers` =
  bits of the layer voices currently sounding for this note.
- **Mid-phrase toggles** (`coupler=on|off`, `sub_octave=on|off` while notes
  are held): toggling ON starts the layer for every currently held note at
  its stored `sounding_velocity`; toggling OFF releases that layer's voices
  immediately (mirrors the predecessor's refreshAudio semantics). The
  bellows reference is untouched.
- **Internal channel setup:** at `init()` (post-sfload) and after every
  `stop` change, channels 14/15 get the current preset (apply_stop loops
  ALL channels) and their layer-gain CC 7 (`apply_layer_gains()`). The
  Phase 2 envelope generators already cover all channels; verified by
  render that `program_select` does NOT reset them (with release_ms=2000
  the coupler tail tracks the main's full 2 s shaped release).
- **Coupler detune (optional, implemented):** channel 15 carries a +3¢
  tuning (`fluid_synth_activate_key_tuning` + `fluid_synth_activate_tuning`,
  MIDI Tuning Standard API, applied once at init — tunings survive program
  changes). Renders show the coupler's fundamental line at f0×2^(+3¢) (e.g.
  555.0 Hz next to the main's 554.0 for note 60) — a subtle beat between
  main and octave layer. The double-stop zones already provide shimmer;
  this is a bonus.
- **Mirroring policy:** incoming PITCH_BEND and CC 11 (expression) are
  mirrored to ACTIVE layer channels so layers track the main voice. **CC 7
  is deliberately NOT mirrored** — on channels 14/15 it IS their fixed gain
  knob (`kCouplerCC7`/`kSubOctaveCC7`); mirroring it would destroy the layer
  balance. All other CCs stay main-channel-only.
- **Behavior fixes shipped with Phase 4** (see "Uniform Bellows Velocity"
  for the state model): duplicate NoteOn on a held key ignored (no
  re-attack — audible change from Phase ≤3); CC 123 hardened to
  `fluid_synth_all_notes_off` on all 16 channels plus the state reset;
  cross-channel NoteOff releases on the stored channel.
- **Verification** (plugin-in-loop renders, objective numbers in
  tests/RESULTS.md Phase 4): octave-up/down lines at exactly ±1 octave; T3
  per-member release removes both the main and coupler voice (no residual);
  T5 drone couplers present with melody bit-identical; T10 duplicate NoteOn
  produces no transient and one release tail; T11 multi-channel CC 123 +
  cross-channel NoteOff strand nothing. Live mid-phrase toggling is
  verified at state level (config tests interleave set_config between
  handle_midi_event calls) plus pre-run-config renders — the renderer has
  no mid-run config mechanism (deliberately: no MIDI semantics invented).
- Startup log line: `Synth layers: coupler=off sub_octave=off (ch15=note+12
  CC7=60, ch14=note-12 CC7=40)`, followed since Phase 5 by
  `Synth drone: off (ch13 CC7=45 vel=100)`.

## Drone (unpika) + config-key registry (Phase 5, 2026-09-19)

A drone is a **fixture**: sustained notes that sound continuously under the
melody, like a real harmonium's drone knobs. They are NOT phrase keys — they
never enter the bellows model (`held_notes_` / `reference_velocity_` are
untouched by drone state, and drone voices are invisible to the duplicate
NoteOn / baton-pass logic).

**Channel 13.** The drone lives on internal FluidSynth channel 13 — the
descending reservation is now 15 coupler, 14 sub-octave, 13 drone, 12 key
click (Phase 6). It plays
the current `stop` preset (`apply_stop()` loops all channels, so a stop
change re-programmes ch13 too; the drone's CC 7 gain is re-asserted
alongside the layer gains). **Channels 12/13/14/15 are all RESERVED**: MIDI
input on ch13 collides with drone voices (same caveat as 12/14/15).

**`drone` config key.** `"off"` (default) or 1–8 comma-separated MIDI note
numbers, e.g. `"48,55"` (Sa+Pa). Strict parsing: digits only — no
whitespace, signs, floats, empty tokens or duplicate notes; more than 8
notes is rejected outright (a real harmonium has a handful of drone knobs);
`""` normalizes to `"off"`; junk → `PLUGIN_INVALID_PARAM` with state
unchanged. Sargam-name parsing ("Sa", "Pa") is future work. Live semantics:
the new spec is diffed against the **actually sounding** notes — added
notes start immediately, removed notes release with the natural
`release_ms` tail, unchanged notes keep sounding (no re-trigger). Diffing
against sounding state (not the stored string) also gives the CC 123
restart semantics below.

**`drone_level` config key.** Integer 0–127 → CC 7 on channel 13, live.
Default **45** — between sub-octave (40) and coupler (60). Measured balance
(see tests/RESULTS.md Phase 5): drone fundamentals −59.5…−60.8 dBFS vs the
melody fundamental line at −45.3…−45.9 dBFS → the drone sits **≈11–14 dB
under the melody** (11 dB with both drone notes power-summed, 14 dB
per-voice); melody lines are bit-identical with the drone on/off, so
nothing is masked. 45 kept as the final default.

**Velocity.** Drone voices start at a FIXED velocity 100
(`kDroneVelocity`); loudness comes solely from CC 7. Drone notes never
interact with the bellows model, never touch `reference_velocity_`, and
duplicate/no-op NoteOn semantics don't apply. State is a separate container
(`std::vector<uint8_t> drone_notes_`), not `held_notes_`.

**CC 123 = full reset.** The hardened handler already ran `all_notes_off`
on all 16 channels (covering ch13); Phase 5 additionally clears
`drone_notes_`. The stored `drone` spec is KEPT in config (a MIDI event
doesn't rewrite config), so re-issuing the same value restarts the notes —
verified in the config harness (audible restart mid-render is impossible:
the renderer has no mid-run config mechanism, deliberately).

**No mirroring.** Pitch bend and CC 11 are mirrored to the coupler/sub
layers (Phase 4) but deliberately NOT to the drone channel: a real drone
knob is independent of the keyboard, so bend/expression must not wobble the
drone. CC 7 was never mirrored anywhere (it IS the drone's gain knob on
ch13).

**Verification** (objective numbers in tests/RESULTS.md Phase 5): T12
melody-over-drone renders (`drone=48,55` vs `drone=off`) — constant drone
lines at 139.5/207.5 Hz THROUGHOUT the on-render (phrase windows AND the
6 s drone-only tail), absent in the off render (noise floor / digital
silence); melody lines bit-identical; drone+`stop=double` render shows the
detune beat on the drone lines too (resolved pair 207.50/208.00 Hz = 0.50 Hz
beat on note 55; merged hump on note 48, its 0.32 Hz beat is below the
window's resolution). T13 — drone + melody + CC 123: everything hits the
s16 floor (−90.3 dBFS) after CC 123 and the synth still plays afterwards.
Log line: `Synth drone: <spec> (ch13 CC7=<level> vel=100)`.

**Config-key registry.** Every `set_config`/`get_config` key (Phases 1–5:
`soundfont_path`, `audio_driver`, `gain`, `reverb`, `chorus`, `attack_ms`,
`release_ms`, `stop`, `coupler`, `sub_octave`, `drone`, `drone_level`) is
now documented authoritatively in **docs/HARMONIUM_CONFIG.md** —
type/format, default, valid range, when it applies, `get_config` echo,
invalid-input behavior, and the backing FluidSynth mechanism.

## Key click + micro-variation (Phase 6, 2026-09-19)

The polish phase: a faint mechanical key noise (chiff) when a pallet opens,
and tiny per-note velocity variation so repeated keys never sound
sample-identical. Defaults keep the Phase 5 sound: `key_click` defaults to
**off** (no audible change unless enabled) and `variation` defaults to
**on** (±1–3 velocity jitter — imperceptible dynamically, but defeats
sample-identical repeats; `variation=off` renders byte-comparable with
Phase 5 behavior).

**Click instrument in the derived font v3.**
`plugins/harmonium/soundfonts/harmonium_v3.sf2` (CMake default; generated by
`tests/scripts/derive_sf2.py --click`, regenerable) = everything v2 has
(presets 0/1; preset 0 proven **byte-identical** to v2 by a T1 render
comparison through the deterministic fluidsynth CLI) PLUS:

- a synthesized "KeyClick" sample appended to sdta: 882 frames (40 ms @
  22050 Hz mono 16-bit, matching the font) — a deterministic brown-noise
  burst (fixed seed; numpy cumsum white noise, detrended), FFT-bandpassed
  700–4000 Hz with raised-cosine edges (sox is unavailable on this machine),
  peak −8 dBFS, 2 ms/15 ms fades. All parameters are module constants in
  `derive_sf2.py`; an audition WAV is written to /tmp/opencode for ear-proxy.
- preset 2 "key click" (bank 0, prog 2) → a one-zone instrument (keys
  21–108 — the click is unpitched mechanical noise; keynum=60 (gen 46)
  fixes the playback rate on every key; NO loop) with a SELF-ENDING volume
  envelope: attackVolEnv 1 ms, holdVolEnv ~0 (−32768 tc), decayVolEnv
  40 ms, sustainVolEnv **1000 cB = 100 dB attenuation = fully closed**,
  releaseVolEnv 15 ms.
- **Self-end design (the critical correctness point):** sustainVolEnv is an
  *attenuation* (0 = hold full level forever, higher = quieter), so the
  voice must DECAY to a silent sustain: the envelope reaches 100 dB down
  1 + 0 + 40 ms after onset, and the unlooped 40 ms sample runs out of data
  at the same time — either mechanism ends the voice ≤60 ms. Measured: the
  burst is audible 502→521 ms after onset at full velocity (≤21 ms, ≪60 ms)
  and the following 4 s hold is s16 digital silence (−90.3 dBFS floor) — a
  self-sustaining click would have been a permanent drone.

**Channel 12 + triggering.** The click layer lives on internal channel 12
(reservation order now 15 coupler, 14 sub-octave, 13 drone, 12 click), plays
preset 2 regardless of the stop preset, and gets a fixed gain CC 7 = 64.
`apply_click_preset()` re-selects preset 2 on ch12 after every
`apply_stop()` (which re-programmes ALL channels), and the channel gains are
asserted AFTER the preset selections (a FAILED program_select — v2 font —
resets the channel's CC 7). `trigger_click()` fires on every ACCEPTED
main-path NoteOn only: drone changes never reach that path (no click), and a
swallowed duplicate NoteOn makes NO click (correct: no pallet moves — proven
by T10 renders with click on: exactly two segments, none at the duplicate
instants). The click is NOT in `held_notes_` (no bellows state) and needs no
noteoff tracking (self-ending envelope). CC 123 simply cuts a sounding click
short (all_notes_off covers ch12; there is no click state to clear). Pitch
bend / CC 11 are deliberately NOT mirrored to ch12 (mechanical noise does
not track expression). **Font guard:** the layer is only armed if preset 2
was actually selected (`click_preset_ok_`); with harmonium_v2.sf2 key_click
is a silent no-op (verified bit-exact vs the plain render) — otherwise the
channel would fall back to the stop preset and stack a quiet duplicate reed
voice on every note.

**`key_click` config key.** `"off"` (default) | `"low"` | `"high"`, strict
validation, pre-init storage OK, live toggle applies from the next accepted
NoteOn. Map: low → click velocity 45, high → 75 (`kClickVelLow` /
`kClickVelHigh`). Measured levels (deterministic fluidsynth CLI renders,
reverb off, gain 0.4 — the exact calls the plugin makes: ch12 CC7=64,
preset 2):

| Mode | Click peak | vs reed onset peak | Active above −70 dBFS |
|---|---|---|---|
| low (vel 45) | −51.5 dBFS | **24.4 dB below** | 3.8–11.7 ms |
| high (vel 75) | −42.6 dBFS | **15.4 dB below** | 3.7–15.7 ms |

"low" is the subtle keyboard chirp; "high" is a clearly audible tick.
Time-domain subtraction (click render minus reed-only render, sample-exact
because the CLI renderer is byte-deterministic) also proves the reed voice
is bit-identical with/without the click (residual after 60 ms = −240 dBFS,
i.e. exact zero) — the click never leaks into the sustain.

**`variation` config key.** `"on"` (default) | `"off"`, strict validation.
When on, a deterministic PRNG (`std::mt19937`, FIXED seed 20260919 =
`kVariationSeed`) jitters ONLY the velocity handed to FluidSynth: ±1..3 on
the main/layers' bellows velocity (`kJitterMain`), ±4..8 on the click
velocity (`kJitterClick`), with an anti-repeat rule (redraws, bounded,
while equal to the previous draw — consecutive notes never get the same
variation). Clamped 1..127. **The bellows model is untouched:** the
reference latch (`reference_velocity_`) and baton-pass bookkeeping use the
RAW press velocities (`HeldNote.velocity`); `HeldNote.played_velocity`
(jittered) is only what FluidSynth hears, and mid-phrase layer toggles
replay the same stored jittered velocity (one finger noise per press).
Audible effect: ±1–3 is imperceptible dynamically (~0.2 dB per velocity
unit at vel 100) but repeated keys are no longer sample-identical.
Measured (T4, 10 consecutive note-60 repeats, per-note segment peaks):
variation=off → all 10 peaks identical to 0.00 dB (Phase 5 behavior);
variation=on → every note −0.30..+0.60 dB from the reference, adjacent
repeats differ 0.20–1.10 dB (never identical). **Determinism:** two renders
of the same config give the same per-note pattern to 0.000 dB (the raw
files still differ in event-to-block placement because the plugin-in-loop
renderer is wall-clock throttled — that is renderer placement jitter, not
plugin nondeterminism; the deterministic fluidsynth CLI renders ARE
byte-identical run-to-run and were used for the click measurements).
Note: the click jitter consumes PRNG draws, so identical CONFIG (not
identical PRNG position) is the reproducibility contract.

**Log line:** `Synth click: key_click=off variation=on (ch12 preset 2
CC7=64, vel low/high=45/75, jitter main +-1..3 click +-1..8, seed
20260919)` — extends the layer/drone log block at startup.

**Verification** (objective numbers in tests/RESULTS.md Phase 6): click
self-end (font-level ≤21 ms burst + 4 s silent hold; CLI-subtraction
residual = exact zero after 60 ms); T4 onsets advance ~2.0 ms with
key_click=high (the click crosses the −55 dB detector threshold before the
reed — 80/80 onsets), T1 high −4.3 ms; T2 legato + T1 with click on stay
clean; T12 drone render unchanged (drone 48/55 lines at −59.5/−60.3 dBFS
identical to Phase 5, melody −45.9 bit-stable, drone-only tail flat — no
clicks from the fixture); T10 swallowed duplicates make no click; v2 font +
key_click = bit-exact no-op; 267/267 config tests; clean build 0 warnings.

**Config-key registry.** `key_click` and `variation` are documented in
docs/HARMONIUM_CONFIG.md (14 keys total).

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
- No CC/passport handling beyond what FluidSynth does natively; no raga
  or GUI features yet. (Uniform bellows velocity IS implemented —
  see "Uniform Bellows Velocity" above; octave coupler + sub-octave
  layers ARE implemented — see "Layer router (Phase 4)"; bellows
  pressure/expression modeling beyond CC#11 mirroring is still future work.)
- **MIDI channels 12/13/14/15 are reserved** by the layer router, drone
  and click layer; MIDI input arriving on them from a controller would
  collide with layer/drone/click voices.

## Suggested next steps

1. **Harmonium realism Phases 7+** (active effort — Phases 0–6 complete):
   - Phase 7 (envelope work can't fix this): high-register stretch — keys
     65–84 are one F4 sample stretched up to +19 semitones; needs new samples
   - Raga note filtering (Phase 8 candidate) in the harmonium plugin (see
     docs/CODEBASE_ANALYSIS.md for the harmonium-companion raga/sargam logic
     worth porting)
   - Low-register beat polish (Phase 3 leftover: raise D or clamp the low
     register — note 43's 0.24 Hz beat is at the slow edge)
   - Click polish (open): the click is currently a fixed bandpassed noise
     burst; a velocity- and register-dependent click (quieter/higher-pitched
     up the keyboard) and a `key_click_level` fine knob are natural
     follow-ups; drone polish: sargam-name parsing for `drone` ("Sa,Pa" →
     48,55 etc., needs a tonic offset decision); optional gentle
     chorus/detune dedicated to the drone channel
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