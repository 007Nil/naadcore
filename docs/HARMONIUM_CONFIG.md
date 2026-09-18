# Harmonium Plugin — Config-Key Registry (authoritative)

Every key accepted by `set_config()` / `get_config()` on the harmonium plugin
(`plugins/harmonium/harmonium_plugin.cpp`), Phases 1–5 combined. This is the
authoritative reference; HANDOVER.md gives the design narrative, this file the
contract. Each row was verified against the implementation (2026-09-19,
Phase 5).

All keys are plugin-only — there is no CLI flag surface yet (see
HANDOVER.md "Suggested next steps"); the offline renderer
(`tests/scripts/run_render_plugin.sh`) can pass any of them as trailing
`KEY=VALUE` pairs applied via `set_config` **before** `init()`.

## Key table

| Key | Type / format | Default | Valid values | When it applies | `get_config` echo | Invalid input | FluidSynth mechanism |
|---|---|---|---|---|---|---|---|
| `soundfont_path` | string (filesystem path) | compiled-in `HARMONIUM_SOUNDFONT_PATH` (in-repo `harmonium_v2.sf2`) | any string (not validated) | **init-only** — used by `sfload` at `init()`; setting it after init does NOT reload | stored string | never rejected; a bad path fails `init()` with `PLUGIN_ERROR` ("Failed to load SoundFont") | `fluid_synth_sfload` |
| `audio_driver` | string | `alsa` | any string (FluidSynth validates at driver creation) | **init-only** — written to settings before the driver is created; the `init(driver)` argument overrides the stored value | stored string | never rejected; an unusable driver fails `start_audio()` with `PLUGIN_ERROR` | `fluid_settings_setstr("audio.driver")` + `new_fluid_audio_driver` |
| `gain` | float | `0.4` (echoed `0.400`) | 0.0 – 10.0 inclusive | **live** | re-reads the synth (`fluid_synth_get_gain`), printed `%.3f` | `PLUGIN_INVALID_PARAM` (junk, trailing chars, empty, NaN/Inf, out of range); state unchanged | `fluid_synth_set_gain` |
| `reverb` | enum | `on` | `on` \| `off` (case-sensitive, exact) | **live** | `on` / `off` | `PLUGIN_INVALID_PARAM`; state unchanged | `fluid_synth_reverb_on(synth, -1, …)` (all groups); room/damp/width/level pinned at init (0.2/0.0/0.3/0.4) |
| `chorus` | enum | `off` | `on` \| `off` (case-sensitive, exact) | **live** | `on` / `off` | `PLUGIN_INVALID_PARAM`; state unchanged | `fluid_synth_chorus_on(synth, -1, …)` |
| `attack_ms` | integer, ms | `10` | 1 – 2000 | **live** (re-applies the generators on all 16 channels) | decimal string | `PLUGIN_INVALID_PARAM` (junk, floats like `10.5`, empty, out of range); state unchanged | `fluid_synth_set_gen` `GEN_VOLENVATTACK` — **additive offset** vs the font's ~1 ms base (−12000 tc); plugin converts ms → compensated offset |
| `release_ms` | integer, ms | `200` | 1 – 4000 | **live** (same re-apply) | decimal string | `PLUGIN_INVALID_PARAM` (same rules) | `fluid_synth_set_gen` `GEN_VOLENVRELEASE` — additive offset vs the font's 100 ms base (−3986 tc) |
| `stop` | enum | `single` | `single` \| `double` (case-sensitive, exact) | **live** — `program_select` on ALL 16 channels, then layer/drone gains re-asserted; re-programming does not kill sounding voices (new notes use the new preset) | stored name | `PLUGIN_INVALID_PARAM` (`quad`, `Double`, `"single "`, empty); state unchanged | `fluid_synth_program_select` preset 0 ("harmonium") / 1 ("harmonium double") of the derived font. Incoming MIDI **PROGRAM_CHANGE events are ignored** (config-controlled stops) |
| `coupler` | enum | `off` | `on` \| `off` (case-sensitive, exact) | **live, mid-phrase** — toggling on starts an octave-up voice (note+12) for EVERY held note at its stored sounding velocity; off releases those voices; bellows reference untouched | `on` / `off` | `PLUGIN_INVALID_PARAM` (`1`, `On`, `"on "`, empty); state unchanged | `fluid_synth_noteon/noteoff` on internal **channel 15** (note+12, silently skipped above note 115), fixed gain CC 7 = 60, +3¢ tuning (`fluid_synth_activate_key_tuning`); pitch bend + CC 11 mirrored to it, CC 7 never |
| `sub_octave` | enum | `off` | `on` \| `off` (case-sensitive, exact) | **live, mid-phrase** — as `coupler` but note−12 on channel 14 | `on` / `off` | `PLUGIN_INVALID_PARAM` (same rules) | `fluid_synth_noteon/noteoff` on internal **channel 14** (note−12, silently skipped below note 12), fixed gain CC 7 = 40; same mirroring policy |
| `drone` | note list | `off` | `off`, or 1–8 comma-separated integers 0–127 — digits only: no whitespace, signs, floats, empty tokens, or duplicate notes. `""` (empty string) = off. Sargam names ("Sa,Pa") are NOT parsed (future work) | **live** — diff against the actually-sounding notes: added notes start immediately, removed notes release (natural `release_ms` tail), unchanged notes keep sounding (no re-trigger). After CC 123 the stored spec is kept but the notes are silenced; **re-issuing the same value restarts them** | canonical spec: `off` (also for `""`) or the accepted value as given | `PLUGIN_INVALID_PARAM` (`sa,pa`, `128`, `-1`, `48.5`, `48,`, `,48`, `48,,55`, `" 48"`, `"48 "`, `"48, 55"`, `OFF`, `on`, 9+ notes, duplicates); state unchanged | `fluid_synth_noteon/noteoff` on internal **channel 13** at fixed velocity 100 (loudness is `drone_level`, not velocity); preset follows `stop`; pitch bend / CC 11 deliberately NOT mirrored; CC 123 silences it via `all_notes_off` + clears the sounding-note container |
| `drone_level` | integer | `45` | 0 – 127 | **live** | decimal string | `PLUGIN_INVALID_PARAM` (junk, floats, empty, trailing space, out of range); state unchanged | `fluid_synth_cc(synth, 13, 7, level)` — CC 7 on channel 13 IS the drone's gain knob (never mirrored from MIDI input). 45 sits between sub-octave (40) and coupler (60); measured ≈11–14 dB under the melody fundamental line (tests/RESULTS.md Phase 5) |

## General notes (apply to every key)

- **Pre-init storage:** `set_config` before `init()` is always stored and
  applied at init (the Phase 3 pattern — `init()` pins the voicing, loads the
  font, then applies the stored config deterministically). This is the only
  path for `soundfont_path`/`audio_driver` and how the offline renderer
  injects config.
- **Unknown keys:** `set_config` → `PLUGIN_NOT_IMPLEMENTED`; `get_config` →
  `""` (empty string, indistinguishable from an unset string key by design).
- **Null arguments:** `set_config(nullptr, …)` or `set_config(key, nullptr)`
  → `PLUGIN_INVALID_PARAM`; `get_config(nullptr)` → `""`.
- **Strict validation everywhere:** enum keys are case-sensitive and reject
  surrounding whitespace; integer/float keys use `strtol`/`strtof` and reject
  trailing junk, floats-on-integer-keys, empty strings, and out-of-range
  values. A rejected `set_config` never changes state.
- **PROGRAM_CHANGE** MIDI events are swallowed (`PLUGIN_OK`, no effect):
  stops are config-controlled. Everything else in `handle_midi_event`
  forwards to FluidSynth on the incoming channel.
- **Reserved internal channels (collision caveat):** 15 = octave coupler,
  14 = sub-octave, 13 = drone. MIDI input arriving on these channels from a
  controller collides with the router/drone voices (the Q49 sends on one
  channel only). The drone (unlike the layers) also never interacts with the
  uniform-bellows state: it is a fixture, not a phrase key.
- **CC 123 (All Notes Off)** on ANY channel: `fluid_synth_all_notes_off` on
  all 16 channels, held-note/bellows state reset, drone sounding-note
  container cleared (see the `drone` row for the restart semantics).

## Version history

- Phase 1 (2026-09-18): `soundfont_path`, `audio_driver`, `gain`, `reverb`,
  `chorus`.
- Phase 2 (2026-09-18): `attack_ms`, `release_ms`.
- Phase 3 (2026-09-19): `stop`.
- Phase 4 (2026-09-19): `coupler`, `sub_octave`.
- Phase 5 (2026-09-19): `drone`, `drone_level`; this registry created.
