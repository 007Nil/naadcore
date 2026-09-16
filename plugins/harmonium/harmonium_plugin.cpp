#include "harmonium_plugin.hpp"
#include <iostream>

// The SoundFont path is embedded at compile time
#ifndef HARMONIUM_SOUNDFONT_PATH
#define HARMONIUM_SOUNDFONT_PATH "/home/nil/harmonium-companion/harmonium.sf2"
#endif

namespace naadcore {

HarmoniumPlugin::HarmoniumPlugin()
    : settings_(nullptr), synth_(nullptr), driver_(nullptr),
      audio_driver_("alsa"), soundfont_path_(HARMONIUM_SOUNDFONT_PATH) {
}

HarmoniumPlugin::~HarmoniumPlugin() {
    stop_audio();
    if (synth_) {
        delete_fluid_synth(synth_);
        synth_ = nullptr;
    }
    if (settings_) {
        delete_fluid_settings(settings_);
        settings_ = nullptr;
    }
}

const PluginInfo* HarmoniumPlugin::get_info() {
    static PluginInfo info = {
        "harmonium",
        "1.0.0",
        "NaadCore Team",
        "FluidSynth-based harmonium synthesizer plugin with embedded SoundFont"
    };
    return &info;
}

PluginResult HarmoniumPlugin::init(const char* audio_driver) {
    // Create settings
    settings_ = new_fluid_settings();
    if (!settings_) {
        std::cerr << "Failed to create FluidSynth settings" << std::endl;
        return PLUGIN_ERROR;
    }
    
    // Create synth
    synth_ = new_fluid_synth(settings_);
    if (!synth_) {
        std::cerr << "Failed to create FluidSynth instance" << std::endl;
        return PLUGIN_ERROR;
    }
    
    // Set audio driver
    if (audio_driver) {
        audio_driver_ = audio_driver;
    }
    fluid_settings_setstr(settings_, "audio.driver", audio_driver_.c_str());
    
    // Load SoundFont (embedded path)
    int sf_id = load_soundfont();
    if (sf_id < 0) {
        std::cerr << "Failed to load SoundFont: " << soundfont_path_ << std::endl;
        return PLUGIN_ERROR;
    }
    
    return PLUGIN_OK;
}

int HarmoniumPlugin::load_soundfont() {
    if (!synth_) {
        return -1;
    }
    
    int id = fluid_synth_sfload(synth_, soundfont_path_.c_str(), true);
    if (id < 0) {
        std::cerr << "Failed to load SoundFont: " << soundfont_path_ << std::endl;
    } else {
        std::cout << "Loaded SoundFont: " << soundfont_path_ << " (ID: " << id << ")" << std::endl;
    }
    return id;
}

PluginResult HarmoniumPlugin::start_audio() {
    if (!synth_) {
        std::cerr << "Synth not initialized" << std::endl;
        return PLUGIN_ERROR;
    }
    
    if (driver_) {
        std::cout << "Audio already running" << std::endl;
        return PLUGIN_OK;
    }
    
    driver_ = new_fluid_audio_driver(settings_, synth_);
    if (!driver_) {
        std::cerr << "Failed to create audio driver: " << audio_driver_ << std::endl;
        return PLUGIN_ERROR;
    }
    
    std::cout << "Audio driver started: " << audio_driver_ << std::endl;
    return PLUGIN_OK;
}

PluginResult HarmoniumPlugin::stop_audio() {
    if (driver_) {
        delete_fluid_audio_driver(driver_);
        driver_ = nullptr;
    }
    return PLUGIN_OK;
}

PluginResult HarmoniumPlugin::handle_midi_event(const MidiEvent& event) {
    if (!synth_) {
        return PLUGIN_ERROR;
    }
    
    switch (event.type) {
        case MidiEvent::NOTE_ON: {
            if (event.data2 > 0) {
                // Note on with velocity
                int result = fluid_synth_noteon(synth_, event.channel, event.data1, event.data2);
                if (result != 0) {
                    std::cerr << "Note on failed: ch=" << (int)event.channel 
                              << " note=" << (int)event.data1 << std::endl;
                    return PLUGIN_ERROR;
                }
            } else {
                // Note on with 0 velocity is note off
                fluid_synth_noteoff(synth_, event.channel, event.data1);
            }
            break;
        }
        
        case MidiEvent::NOTE_OFF: {
            fluid_synth_noteoff(synth_, event.channel, event.data1);
            break;
        }
        
        case MidiEvent::CONTROL_CHANGE: {
            fluid_synth_cc(synth_, event.channel, event.data1, event.data2);
            break;
        }
        
        case MidiEvent::PITCH_BEND: {
            int value = event.data1 | (event.data2 << 7);
            fluid_synth_pitch_bend(synth_, event.channel, value);
            break;
        }
        
        case MidiEvent::PROGRAM_CHANGE: {
            fluid_synth_program_change(synth_, event.channel, event.data1);
            break;
        }
        
        case MidiEvent::CHANNEL_PRESSURE: {
            fluid_synth_channel_pressure(synth_, event.channel, event.data1);
            break;
        }
        
        case MidiEvent::KEY_PRESSURE: {
            fluid_synth_key_pressure(synth_, event.channel, event.data1, event.data2);
            break;
        }
        
        default:
            // Unsupported event type
            break;
    }
    
    return PLUGIN_OK;
}

std::string HarmoniumPlugin::get_config(const char* key) {
    if (!key) return "";
    
    std::string k = key;
    
    if (k == "soundfont_path") {
        return soundfont_path_;
    }
    if (k == "audio_driver") {
        return audio_driver_;
    }
    
    return "";
}

PluginResult HarmoniumPlugin::set_config(const char* key, const char* value) {
    if (!key || !value) {
        return PLUGIN_INVALID_PARAM;
    }
    
    std::string k = key;
    
    if (k == "soundfont_path") {
        soundfont_path_ = value;
        return PLUGIN_OK;
    }
    if (k == "audio_driver") {
        audio_driver_ = value;
        return PLUGIN_OK;
    }
    
    return PLUGIN_NOT_IMPLEMENTED;
}

PluginResult HarmoniumPlugin::set_soundfont_path(const std::string& path) {
    soundfont_path_ = path;
    return PLUGIN_OK;
}

} // namespace naadcore

// ============================================================================
// C-linkage factory functions (exported by plugin)
// ============================================================================

extern "C" {

naadcore::INaadPlugin* naad_plugin_create() {
    return new naadcore::HarmoniumPlugin();
}

void naad_plugin_destroy(naadcore::INaadPlugin* plugin) {
    delete plugin;
}

int naad_plugin_get_version() {
    return 1;
}

} // extern "C"
