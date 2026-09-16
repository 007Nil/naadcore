#ifndef NAADCORE_PLUGIN_HARMONIUM_HPP
#define NAADCORE_PLUGIN_HARMONIUM_HPP

#include "naadcore/plugin.hpp"
#include <fluidsynth.h>
#include <string>
#include <memory>

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
    
    // midi channel state
    int channel_ = 0;
};

} // namespace naadcore

#endif // NAADCORE_PLUGIN_HARMONIUM_HPP
