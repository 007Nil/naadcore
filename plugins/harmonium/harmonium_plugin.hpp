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

    // Uniform bellows velocity: a real harmonium's bellows drive all open
    // reeds at the same pressure, so keys pressed together sound at the
    // first key's velocity. Each held key remembers its original press
    // velocity in press order; when the reference (oldest) key is
    // released, the baton passes to the next oldest held key's original
    // velocity. Already-sounding notes are never re-velocityed. Held-note
    // state ignores MIDI channel (one harmonium, one bellows).
    struct HeldNote {
        uint8_t note;
        uint8_t velocity;  ///< original press velocity
    };
    std::vector<HeldNote> held_notes_;  ///< front() = oldest pressed
    uint8_t reference_velocity_ = 0;

    std::vector<HeldNote>::iterator find_held(uint8_t note);
    void release_held_note(uint8_t note);

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
};

} // namespace naadcore

#endif // NAADCORE_PLUGIN_HARMONIUM_HPP
