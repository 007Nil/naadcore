#ifndef NAADCORE_PLUGIN_HARMONIUM_HPP
#define NAADCORE_PLUGIN_HARMONIUM_HPP

#include "naadcore/plugin.hpp"
#include <fluidsynth.h>
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
    //   channel 15 = octave coupler  (note + 12, ~6 dB below main)
    //   channel 14 = sub-octave      (note - 12, quieter still)
    // Channel 7 (volume) on the internal channels IS their gain knob, so
    // incoming CC 7 is never mirrored to them; CC 11 (expression) and
    // pitch bend are mirrored to active layers. Channels 14/15 are thus
    // RESERVED: MIDI input arriving on them from a controller would
    // collide with the layer router.
    static constexpr uint8_t kLayerCoupler = 0x1;   ///< HeldNote.layers bit
    static constexpr uint8_t kLayerSubOctave = 0x2; ///< HeldNote.layers bit
    static constexpr int kCouplerChannel = 15;
    static constexpr int kSubOctaveChannel = 14;
    /// Channel-volume (CC 7) layer gains. CC 7 maps to initial-attenuation
    /// via FluidSynth's default modulator (960 cB at CC7=0, linear in the
    /// source): the values below were tuned against plugin-in-loop renders
    /// (see HANDOVER.md "Layer router" for the measured dB vs main).
    static constexpr int kCouplerCC7 = 60;
    static constexpr int kSubOctaveCC7 = 40;
    /// Optional coupler detune (+3 cents, coupler channel only) via the
    /// MIDI Tuning Standard API — subtle beat between main and octave
    /// layer. The double-stop zones already shimmer; this is a bonus.
    static constexpr double kCouplerDetuneCents = 3.0;

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

    // Uniform bellows velocity: a real harmonium's bellows drive all open
    // reeds at the same pressure, so keys pressed together sound at the
    // first key's velocity. Each held key remembers its original press
    // velocity in press order; when the reference (oldest) key is
    // released, the baton passes to the next oldest held key's original
    // velocity. Already-sounding notes are never re-velocityed. Held-note
    // state ignores MIDI channel (one harmonium, one bellows).
    struct HeldNote {
        uint8_t note;
        uint8_t velocity;             ///< original press velocity
        uint8_t channel;              ///< incoming channel of the main voice
        uint8_t sounding_velocity;    ///< velocity actually played (reference at press time)
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
    /// (mid-phrase coupler/sub_octave config toggle). Each note sounds at
    /// its stored sounding_velocity; the bellows reference is untouched.
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

    /// Create a +3 cents tuning and activate it on the coupler channel
    /// only (once at init; tunings survive program changes). Silent no-op
    /// if the synth is not ready or the API call fails.
    void apply_coupler_detune();

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
};

} // namespace naadcore

#endif // NAADCORE_PLUGIN_HARMONIUM_HPP
