#ifndef NAADCORE_PLUGIN_PIANO_HPP
#define NAADCORE_PLUGIN_PIANO_HPP

#include "naadcore/plugin.hpp"
#include <fluidsynth.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file piano_plugin.hpp
 * @brief Piano plugin implementation
 *
 * This plugin loads a grand piano SoundFont (Salamander Grand Piano) via
 * FluidSynth and provides standard piano note-on/note-off behavior with
 * velocity-sensitive sound. No harmonium-specific features (bellows, stops,
 * drones, key-click).
 */

namespace naadcore {

/**
 * @brief Piano plugin implementation
 *
 * A simple FluidSynth-based piano instrument. Each accepted NoteOn starts a
 * voice at the incoming velocity; each NoteOff releases it. No bellows model,
 * no reed stops, no drones — standard piano behavior.
 */
class PianoPlugin : public naadcore::INaadPlugin {
public:
    PianoPlugin();
    ~PianoPlugin() override;

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
    std::string audio_device_;
    std::string soundfont_path_;
    int soundfont_id_;  ///< ID of the loaded SoundFont (-1 until loaded)
};

} // namespace naadcore

#endif // NAADCORE_PLUGIN_PIANO_HPP
