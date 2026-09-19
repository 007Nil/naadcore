#include "piano_plugin.hpp"
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>

// The SoundFont path is embedded at compile time.
// Fallback (only for builds that bypass CMake and therefore do not define
// PIANO_SOUNDFONT_PATH): the in-repo default font, relative to the
// repository ROOT — non-CMake builds must run the plugin from the repo
// root for this fallback to resolve. CMake builds pass the absolute path
// instead, so this literal is compiled out there.
#ifndef PIANO_SOUNDFONT_PATH
#define PIANO_SOUNDFONT_PATH "plugins/piano/soundfonts/SalamanderGrandLite.sf2"
#endif

namespace naadcore {

PianoPlugin::PianoPlugin()
    : settings_(nullptr), synth_(nullptr), driver_(nullptr),
      audio_driver_("alsa"), soundfont_path_(PIANO_SOUNDFONT_PATH),
      soundfont_id_(-1) {
}

PianoPlugin::~PianoPlugin() {
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

const PluginInfo* PianoPlugin::get_info() {
    static PluginInfo info = {
        "piano",
        "1.0.0",
        "NaadCore Team",
        "FluidSynth-based grand piano instrument using the Salamander Grand Piano SoundFont"
    };
    return &info;
}

PluginResult PianoPlugin::init(const char* audio_driver) {
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

    // Audio output device (well-known "audio_device" config key, applied
    // by the host via set_config BEFORE init). Mapped per driver at init.
    if (!audio_device_.empty()) {
        if (audio_driver_ == "alsa") {
            fluid_settings_setstr(settings_, "audio.alsa.device",
                                  audio_device_.c_str());
        } else if (audio_driver_ == "pulseaudio") {
            fluid_settings_setstr(settings_, "audio.pulseaudio.device",
                                  audio_device_.c_str());
        } else if (audio_driver_ == "pipewire") {
            std::cerr << "Warning: audio_device '" << audio_device_
                      << "' ignored — no device setting exists in "
                      << "FluidSynth's pipewire driver" << std::endl;
        }
        // "file" and any other driver: silently ignored.
    }
    std::cout << "Synth audio: driver=" << audio_driver_
              << " device=" << (!audio_device_.empty() ? audio_device_ : "default")
              << std::endl;

    // Synth voicing: modest reverb, 4th-order interpolation.
    fluid_synth_set_gain(synth_, 0.5f);
    fluid_synth_reverb_on(synth_, -1, 1);
    fluid_synth_set_reverb_group_roomsize(synth_, -1, 0.2);
    fluid_synth_set_reverb_group_damp(synth_, -1, 0.0);
    fluid_synth_set_reverb_group_width(synth_, -1, 0.3);
    fluid_synth_set_reverb_group_level(synth_, -1, 0.4);
    fluid_synth_chorus_on(synth_, -1, 0);
    int midi_channels = fluid_synth_count_midi_channels(synth_);
    for (int ch = 0; ch < midi_channels; ++ch) {
        fluid_synth_set_interp_method(synth_, ch, FLUID_INTERP_4THORDER);
    }
    std::cout << "Synth voicing: gain=0.5 reverb=on chorus=off interp=4th-order"
              << std::endl;

    // Soften the hammer transient (key-click/chiff) baked into the sample
    // by increasing the volume-envelope attack time. The SF2 default is
    // ~1 ms (GEN_VOLENVATTACK = -12000 timecents), which lets the hammer
    // hit come through fully. A 10 ms ramp hides most of the transient
    // while keeping the piano responsive.
    //
    // 10 ms in timecents = 1200 * log2(10/1000) = -7972.6 tc
    // Zone attack = -12000 (SF2 default), so offset = -7972.6 - (-12000) = +4027
    for (int ch = 0; ch < midi_channels; ++ch) {
        fluid_synth_set_gen(synth_, ch, GEN_VOLENVATTACK, 4027.0f);
    }

    // Load SoundFont (embedded path)
    soundfont_id_ = load_soundfont();
    if (soundfont_id_ < 0) {
        std::cerr << "Failed to load SoundFont: " << soundfont_path_ << std::endl;
        return PLUGIN_ERROR;
    }

    // Deterministic program state: select preset 0 on all channels (grand
    // piano is typically the default preset, but be explicit).
    const int midi_channels_after_load = fluid_synth_count_midi_channels(synth_);
    for (int ch = 0; ch < midi_channels_after_load; ++ch) {
        fluid_synth_program_select(synth_, ch, soundfont_id_, 0, 0);
    }

    return PLUGIN_OK;
}

int PianoPlugin::load_soundfont() {
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

PluginResult PianoPlugin::start_audio() {
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

PluginResult PianoPlugin::stop_audio() {
    if (driver_) {
        delete_fluid_audio_driver(driver_);
        driver_ = nullptr;
    }
    return PLUGIN_OK;
}

PluginResult PianoPlugin::handle_midi_event(const MidiEvent& event) {
    if (!synth_) {
        return PLUGIN_ERROR;
    }

    switch (event.type) {
        case MidiEvent::NOTE_ON: {
            if (event.data2 > 0) {
                // Standard piano: velocity-sensitive note on.
                int result = fluid_synth_noteon(synth_, event.channel,
                                                 event.data1, event.data2);
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

            // Handle All Notes Off (CC 123) — standard MIDI behavior.
            if (event.data1 == 123) {
                const int midi_channels = fluid_synth_count_midi_channels(synth_);
                for (int ch = 0; ch < midi_channels; ++ch) {
                    fluid_synth_all_notes_off(synth_, ch);
                }
            }
            break;
        }

        case MidiEvent::PITCH_BEND: {
            int value = event.data1 | (event.data2 << 7);
            fluid_synth_pitch_bend(synth_, event.channel, value);
            break;
        }

        case MidiEvent::PROGRAM_CHANGE: {
            // Standard piano: allow program changes so the user can switch
            // piano presets if the SoundFont has multiple (e.g. grand,
            // upright, electric piano). Forward to FluidSynth.
            fluid_synth_program_change(synth_, event.channel, event.data1);
            break;
        }

        case MidiEvent::CHANNEL_PRESSURE: {
            fluid_synth_channel_pressure(synth_, event.channel, event.data1);
            break;
        }

        case MidiEvent::KEY_PRESSURE: {
            fluid_synth_key_pressure(synth_, event.channel, event.data1,
                                     event.data2);
            break;
        }

        default:
            // Unsupported event type — silently ignore
            break;
    }

    return PLUGIN_OK;
}

std::string PianoPlugin::get_config(const char* key) {
    if (!key) return "";

    std::string k = key;

    if (k == "soundfont_path") {
        return soundfont_path_;
    }
    if (k == "audio_driver") {
        return audio_driver_;
    }
    if (k == "audio_device") {
        return audio_device_;
    }

    return "";
}

PluginResult PianoPlugin::set_config(const char* key, const char* value) {
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
    if (k == "audio_device") {
        audio_device_ = value;
        return PLUGIN_OK;
    }

    return PLUGIN_NOT_IMPLEMENTED;
}

PluginResult PianoPlugin::set_soundfont_path(const std::string& path) {
    soundfont_path_ = path;
    return PLUGIN_OK;
}

} // namespace naadcore

// ============================================================================
// C-linkage factory functions (exported by plugin)
// ============================================================================

extern "C" {

naadcore::INaadPlugin* naad_plugin_create() {
    return new naadcore::PianoPlugin();
}

void naad_plugin_destroy(naadcore::INaadPlugin* plugin) {
    delete plugin;
}

int naad_plugin_get_version() {
    return 1;
}

} // extern "C"
