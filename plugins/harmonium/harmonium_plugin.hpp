#ifndef NAADCORE_PLUGIN_HARMONIUM_HPP
#define NAADCORE_PLUGIN_HARMONIUM_HPP

#include "naadcore/plugin.hpp"
#include <fluidsynth.h>
#include <random>
#include <string>
#include <memory>
#include <vector>

/**
 * @file harmonium_plugin.hpp
 * @brief Harmonium plugin implementation
 * 
 * This plugin embeds the SoundFont path at compile time.
 * The SoundFont is NOT loaded dynamically.
 */

namespace naadcore {

/**
 * @brief Harmonium plugin implementation
 */
class HarmoniumPlugin : public naadcore::INaadPlugin {
public:
    HarmoniumPlugin();
    ~HarmoniumPlugin() override;
    
    // INaadPlugin interface
    const PluginInfo* get_info() override;
    PluginResult init(const char* audio_driver = nullptr) override;
    PluginResult start_audio() override;
    PluginResult stop_audio() override;
    PluginResult handle_midi_event(const MidiEvent& event) override;
    std::string get_config(const char* key) override;
    PluginResult set_config(const char* key, const char* value) override;
    
    // Plugin-specific methods
    PluginResult set_soundfont_path(const std::string& path);
    int load_soundfont();
    
private:
    fluid_settings_t* settings_;
    fluid_synth_t* synth_;
    fluid_audio_driver_t* driver_;
    std::string audio_driver_;
    // Audio output device (well-known "audio_device" config key, applied
    // by the host via the config seam BEFORE init()). "" = unset → the
    // FluidSynth driver's own default device. Stored verbatim, NO
    // validation at set time (device names are machine-specific — ALSA
    // PCM names, PulseAudio sink names); the real validator is
    // start_audio(), where a genuinely unusable device fails
    // new_fluid_audio_driver() with the clear error. Mapped per driver at
    // init(): alsa → "audio.alsa.device", pulseaudio →
    // "audio.pulseaudio.device", file → ignored silently (the offline
    // renderer owns "audio.file.name"), pipewire → ignored with a warning
    // (no device setting exists in FluidSynth's pipewire driver).
    // Applies at start only (init-only semantics, like audio_driver).
    std::string audio_device_;
    std::string soundfont_path_;
    int soundfont_id_;  ///< ID of the loaded SoundFont (-1 until loaded)

    float gain_ = 0.4f;
    bool reverb_on_ = true;
    bool chorus_on_ = false;

    // Volume-envelope shaping (Phase 2). The SF2 has an ~1 ms instant
    // attack (clicky reed speech) and a 100 ms explicit release; these
    // defaults soften the speech to 10 ms and stretch the bellows tail
    // to 200 ms via FluidSynth channel generators (set_gen values are
    // additive offsets — see HANDOVER.md "Volume-envelope shaping" for
    // the calibrated override-vs-additive finding).
    int attack_ms_ = 10;
    int release_ms_ = 200;

    // Reed stops (Phase 3). "single" = preset 0 of the loaded SoundFont
    // (today's sound); "double" = preset 1, two slightly-detuned unison
    // reeds per note (the signature slow beating / shimmer). The stop is
    // config-controlled ONLY: incoming MIDI PROGRAM_CHANGE events are
    // deliberately ignored so a stray program change cannot wreck the
    // voicing (see HANDOVER.md "Reed stops").
    std::string stop_ = "single";

    // Layer router (Phase 4). Each held note can additionally sound on
    // fixed internal FluidSynth channels, all on the current stop preset:
    //   channel 15 = octave coupler  (note + 12, at PARITY with main)
    //   channel 14 = sub-octave      (note - 12, quieter)
    // Channel 7 (volume) on the internal channels IS their gain knob, so
    // incoming CC 7 is never mirrored to them; CC 11 (expression) and
    // pitch bend are mirrored to active layers. Channels 12-15 are thus
    // RESERVED (15 coupler, 14 sub-octave, 13 drone, 12 key click):
    // MIDI input arriving on them from a controller would collide with
    // the layer router / drone / click layer.
    static constexpr uint8_t kLayerCoupler = 0x1;   ///< HeldNote.layers bit
    static constexpr uint8_t kLayerSubOctave = 0x2; ///< HeldNote.layers bit
    static constexpr int kCouplerChannel = 15;
    static constexpr int kSubOctaveChannel = 14;
    /// Channel-volume (CC 7) layer gains. The COUPLER is at PARITY with the
    /// main voice (user spec, 2026-09-20): kCouplerCC7 = 100 is FluidSynth's
    /// default channel volume — the same value the untouched main channels
    /// sit at — and the coupler voice is started at the note's own played
    /// velocity, so the octave sounds exactly like the same key pressed one
    /// octave up: no separate level curve, no detune, exactly +12 semitones.
    /// (Was CC7=60 ≈ −9 dB + a +3¢ detune until 2026-09-20 — audible but
    /// far too subtle to read as octave doubling; see HANDOVER.md "Layer
    /// router".) The sub-octave keeps its fixed background-layer gain.
    static constexpr int kCouplerCC7 = 100;
    static constexpr int kSubOctaveCC7 = 40;

    bool coupler_on_ = false;
    bool sub_octave_on_ = false;

    // Drone (Phase 5). A drone is a FIXTURE: sustained notes that sound
    // continuously under the melody, like a real harmonium's drone knobs —
    // they are NOT phrase keys. The drone lives on internal channel 13
    // (descending reservation: 15 coupler, 14 sub-octave, 13 drone), plays
    // the current stop preset (apply_stop() covers it), and never touches
    // the bellows model: drone voices are not in held_notes_, never read
    // or write reference_velocity_, and start at a FIXED velocity —
    // loudness comes solely from the CC 7 gain below (drone_level key).
    // Pitch bend and CC 11 are deliberately NOT mirrored to channel 13
    // (a drone knob is independent of the keyboard, unlike the coupler/
    // sub layers which track the keys). Channel 13 is RESERVED like
    // 14/15: MIDI input arriving on it collides with drone voices.
    static constexpr int kDroneChannel = 13;
    /// Fixed note-on velocity for drone voices (velocity is NOT the
    /// drone's volume control; the CC 7 gain is).
    static constexpr uint8_t kDroneVelocity = 100;
    /// Maximum number of simultaneous drone notes the "drone" key
    /// accepts (a real harmonium has a handful of drone knobs); a spec
    /// with more tokens is rejected outright.
    static constexpr size_t kDroneMaxNotes = 8;
    /// Default drone gain (CC 7 on channel 13), between the sub-octave
    /// (40) and coupler (60) layer gains. Tuned against plugin-in-loop
    /// renders so the drone sits clearly under the melody (see
    /// HANDOVER.md "Drone" for the measured margin).
    static constexpr int kDroneCC7 = 45;

    /// Canonical "drone" config echo: "off" or the accepted note list.
    std::string drone_spec_ = "off";
    /// Notes currently sounding on channel 13 (CC 123 clears this; the
    /// stored spec is kept, so re-issuing the same value restarts them).
    std::vector<uint8_t> drone_notes_;
    /// 0..127, sent as CC 7 on channel 13 (drone_level key).
    int drone_level_ = kDroneCC7;

    // Key click / chiff (Phase 6). A real harmonium's key makes a faint
    // mechanical noise as the pallet opens. The derived font v3 carries a
    // SELF-ENDING click instrument (preset 2) — the voice dies <= 60 ms
    // after onset via its envelope + end-of-sample data, so NO noteoff
    // tracking is needed. It lives on internal channel 12 (reservation
    // order is now 15 coupler, 14 sub-octave, 13 drone, 12 click), plays
    // ONLY on accepted main-path NoteOns (never for drone changes and
    // never for a swallowed duplicate NoteOn — a swallowed duplicate means
    // no pallet moves, so no click), is NOT in held_notes_ (no bellows
    // state), and is NOT mirrored bend/CC11 (a mechanical noise does not
    // track expression). Level = fixed CC 7 below x velocity from the
    // key_click mode ("low"/"high"); the self-ending envelope does the rest.
    static constexpr int kClickChannel = 12;
    static constexpr int kClickPreset = 2;
    /// Fixed channel gain for the click layer (CC 7 on channel 12 IS its
    /// gain knob, like the layer gains — never mirrored from MIDI input).
    static constexpr int kClickCC7 = 64;
    /// key_click=low / key_click=high trigger velocities (faint transient;
    /// final constants tuned against plugin-in-loop renders, see
    /// tests/RESULTS.md Phase 6 for the measured transient levels).
    static constexpr uint8_t kClickVelLow = 45;
    static constexpr uint8_t kClickVelHigh = 75;
    /// "off" (default) | "low" | "high". Stored pre-init, consulted per
    /// NoteOn, so a live toggle applies from the next accepted NoteOn.
    std::string key_click_ = "off";
    /// True only after the click preset was successfully selected on
    /// channel 12. A font without preset 2 (e.g. harmonium_v2.sf2) fails
    /// the selection; trigger_click() then stays a no-op — otherwise the
    /// channel would fall back to the stop preset and key_click would
    /// stack a quiet duplicate reed voice on every note.
    bool click_preset_ok_ = false;

    // Per-note micro-variation (Phase 6). A deterministic PRNG (FIXED seed
    // — determinism keeps offline renders byte-reproducible) jitters the
    // velocity handed to FluidSynth on every accepted NoteOn: +-1..3 on
    // the main/layers' bellows velocity, +-4..8 on the click velocity.
    // The jitter perturbs ONLY the played velocity — the bellows reference
    // latch / baton-pass bookkeeping stays EXACT (reference_velocity_ and
    // HeldNote.velocity hold raw press velocities). Audible effect: +-1..3
    // is imperceptible dynamically (well under 0.5 dB) but defeats
    // sample-identical repeats of the same key.
    static constexpr uint32_t kVariationSeed = 20260919u;
    static constexpr int kJitterMain = 3;   ///< +-1..kMain on main/layers
    static constexpr int kJitterClick = 8;  ///< +-1..kClick on click layer
    /// "on" (default) | "off"; off = exact velocities (byte-comparable
    /// regression against Phase 5).
    bool variation_on_ = true;
    std::mt19937 rng_{kVariationSeed};
    /// Last jitter draws (main / click). The anti-repeat rule redraws
    /// while equal to the previous draw, so two consecutive notes never
    /// get the same variation (bounded to a few redraws; |j| >= 1 > 0, so
    /// the initial 0 can never match).
    int last_main_jitter_ = 0;
    int last_click_jitter_ = 0;

    // Uniform bellows velocity: a real harmonium's bellows drive all open
    // reeds at the same pressure, so keys pressed together sound at the
    // first key's velocity. Each held key remembers its original press
    // velocity in press order; when the reference (oldest) key is
    // released, the baton passes to the next oldest held key's original
    // velocity. Already-sounding notes are never re-velocityed. Held-note
    // state ignores MIDI channel (one harmonium, one bellows).
    struct HeldNote {
        uint8_t note;
        uint8_t velocity;             ///< original press velocity (raw, bellows bookkeeping)
        uint8_t channel;              ///< incoming channel of the main voice
        uint8_t sounding_velocity;    ///< bellows reference at press time (exact)
        uint8_t played_velocity;      ///< sounding_velocity after variation jitter (what FluidSynth heard)
        uint8_t layers;               ///< kLayer* bits: internal voices started for this note
    };
    std::vector<HeldNote> held_notes_;  ///< front() = oldest pressed
    uint8_t reference_velocity_ = 0;

    std::vector<HeldNote>::iterator find_held(uint8_t note);
    /// Release ALL voices of a held note (main on its stored channel +
    /// coupler/sub on the fixed internal channels), erase it from
    /// held_notes_ and run the baton-pass bookkeeping. Returns false if
    /// the note was not held (caller may pass the event through as-is).
    bool release_held_note(uint8_t note);

    /// Start (on=true) or release (on=false) one layer's voice for a held
    /// note, keeping its layers bit in sync. Range clamps: a coupler voice
    /// above MIDI note 127 or a sub voice below 0 is silently skipped.
    void set_note_layer(HeldNote& held, uint8_t layer_bit, bool on);

    /// Start/release a layer's voice for EVERY currently held note
    /// (mid-phrase sub_octave config toggle — the COUPLER key deliberately
    /// does NOT use this: its new state applies to new presses only, per
    /// the user spec; see set_config). Each note sounds at its stored
    /// played_velocity; the bellows reference is untouched.
    void set_layer_for_all_held(uint8_t layer_bit, bool on);

    /// Apply attack_ms_/release_ms_ as volume-envelope generators on all
    /// MIDI channels (no-op before init(); applied there and on live
    /// set_config changes). Safe to call before the SoundFont is loaded —
    /// channel generators survive sfload and program resets.
    void apply_envelope_gens();

    /// Map the stop name to its SoundFont preset index ("single" -> 0,
    /// "double" -> 1). Returns -1 for unknown names (callers validate
    /// before storing, so this is defensive).
    static int stop_preset_index(const std::string& stop);

    /// fluid_synth_program_select the stop's preset on ALL MIDI channels
    /// (no-op before the synth + SoundFont are ready).
    void apply_stop();

    /// Set the layer-gain CC 7 values on the internal channels (called at
    /// init and after every stop change, next to apply_stop()).
    void apply_layer_gains();

    /// Validate + parse a "drone" value into note numbers. "off" and ""
    /// both parse to an empty list; anything else must be a comma-
    /// separated list of integers 0-127 (strict: digits only — no
    /// whitespace, signs, floats, empty tokens or duplicate notes; at
    /// most kDroneMaxNotes notes). Returns false on invalid input
    /// (caller replies PLUGIN_INVALID_PARAM and keeps the old spec).
    static bool parse_drone_spec(const std::string& value,
                                 std::vector<uint8_t>& out);

    /// Start one drone voice on channel 13 at the fixed drone velocity
    /// and record it in drone_notes_. A failed noteon (no font zone for
    /// the note) is logged and NOT recorded.
    void start_drone_note(uint8_t note);

    /// Reconcile the sounding drone voices (drone_notes_) with the
    /// stored spec: newly added notes start immediately, removed notes
    /// release (natural release_ms tail), unchanged notes keep sounding
    /// without re-trigger. The diff runs against the SOUNDING state, so
    /// after CC 123 re-issuing the same spec restarts every note.
    /// No-op before the synth + SoundFont are ready (init applies it).
    void apply_drone_spec();

    /// Send drone_level_ as CC 7 on channel 13 — the drone's gain knob
    /// (no-op before init; called at init and on live changes).
    void apply_drone_level();

    /// program_select the key-click preset (2) on internal channel 12.
    /// apply_stop() re-programmes ALL channels (including 12) to the stop
    /// preset, so this must run after every apply_stop() — at init and on
    /// live stop changes. A font without preset 2 (e.g. harmonium_v2.sf2)
    /// fails the selection gracefully: key_click then simply stays silent.
    void apply_click_preset();

    /// Jitter a velocity by a random magnitude in 1..max_jitter with a
    /// random sign (never zero — a varied note never plays at the exact
    /// input velocity), clamped to 1..127. Anti-repeat: redraws (bounded)
    /// while the draw equals last_jitter, so consecutive notes never get
    /// the same variation. Deterministic: uses the fixed-seed mt19937
    /// stream. ONLY the value handed to FluidSynth is perturbed; bellows
    /// bookkeeping uses the raw velocities.
    uint8_t jitter_velocity(int velocity, int max_jitter, int& last_jitter);

    /// Trigger the key-click voice on channel 12 (key_click mode -> base
    /// velocity, +-kJitterClick variation). Fire-and-forget: the click
    /// preset's envelope self-ends the voice, no noteoff tracking.
    void trigger_click(uint8_t note);
};

} // namespace naadcore

#endif // NAADCORE_PLUGIN_HARMONIUM_HPP
