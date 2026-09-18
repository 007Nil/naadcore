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
};

} // namespace naadcore

#endif // NAADCORE_PLUGIN_HARMONIUM_HPP
