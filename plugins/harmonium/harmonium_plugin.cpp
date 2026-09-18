#include "harmonium_plugin.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// The SoundFont path is embedded at compile time
#ifndef HARMONIUM_SOUNDFONT_PATH
#define HARMONIUM_SOUNDFONT_PATH "/home/nil/harmonium-companion/harmonium.sf2"
#endif

namespace naadcore {

namespace {

// Volume-envelope bases baked into harmonium.sf2 (docs/HARMONIUM_SF2_AUDIT.md):
// attackVolEnv is the SF2 default (~1 ms, -12000 timecents), releaseVolEnv is
// explicitly 100 ms (-3986 timecents), sustain is full (0 cB attenuation).
constexpr double kSf2AttackTc = -12000.0;   ///< ~1 ms (SF2 default)
constexpr double kSf2ReleaseTc = -3986.0;   ///< 100 ms (explicit in the font)

// SF2 envelope time generators are in timecents: tc = 1200 * log2(seconds).
double ms_to_timecents(double ms) {
    return 1200.0 * std::log2(ms / 1000.0);
}

} // namespace

HarmoniumPlugin::HarmoniumPlugin()
    : settings_(nullptr), synth_(nullptr), driver_(nullptr),
      audio_driver_("alsa"), soundfont_path_(HARMONIUM_SOUNDFONT_PATH),
      soundfont_id_(-1) {
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

    // Synth voicing: modest reverb ("small room"), no chorus, 4th-order
    // interpolation. Gain/reverb/chorus are live-adjustable via set_config.
    fluid_synth_set_gain(synth_, gain_);
    fluid_synth_reverb_on(synth_, -1, reverb_on_ ? 1 : 0);
    fluid_synth_set_reverb_group_roomsize(synth_, -1, 0.2);
    fluid_synth_set_reverb_group_damp(synth_, -1, 0.0);
    fluid_synth_set_reverb_group_width(synth_, -1, 0.3);
    fluid_synth_set_reverb_group_level(synth_, -1, 0.4);
    fluid_synth_chorus_on(synth_, -1, chorus_on_ ? 1 : 0);
    int midi_channels = fluid_synth_count_midi_channels(synth_);
    for (int ch = 0; ch < midi_channels; ++ch) {
        fluid_synth_set_interp_method(synth_, ch, FLUID_INTERP_4THORDER);
    }
    std::cout << "Synth voicing: gain=" << gain_
              << " reverb=" << (reverb_on_ ? "on" : "off")
              << " chorus=" << (chorus_on_ ? "on" : "off")
              << " interp=4th-order" << std::endl;

    // Volume-envelope shaping (Phase 2): soften the reed speech (10 ms ramp
    // over the font's clicky ~1 ms attack) and stretch the release into a
    // breathier bellows tail (200 ms over the font's 100 ms). Live-adjustable
    // via the attack_ms / release_ms config keys.
    apply_envelope_gens();
    std::cout << "Synth envelope: attack_ms=" << attack_ms_
              << " release_ms=" << release_ms_ << std::endl;

    // Load SoundFont (embedded path)
    soundfont_id_ = load_soundfont();
    if (soundfont_id_ < 0) {
        std::cerr << "Failed to load SoundFont: " << soundfont_path_ << std::endl;
        return PLUGIN_ERROR;
    }

    // Deterministic program state: sfload's reset behavior is not part of
    // the contract, so select preset 0 explicitly on all channels, then
    // apply the configured stop on top.
    const int midi_channels_after_load = fluid_synth_count_midi_channels(synth_);
    for (int ch = 0; ch < midi_channels_after_load; ++ch) {
        fluid_synth_program_select(synth_, ch, soundfont_id_, 0, 0);
    }
    apply_stop();
    std::cout << "Synth stop: " << stop_ << std::endl;

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

void HarmoniumPlugin::apply_envelope_gens() {
    if (!synth_) {
        return;
    }
    // FluidSynth 2.4 applies fluid_synth_set_gen() values as channel-gen
    // OFFSETS added on top of the instrument zone's generator values
    // (calibrated empirically 2026-09-18 against the ~100 ms SF2 release,
    // raw file-driver renders measured with tests/analyze.py: setting 0
    // left the release at 99 ms, +1200 gave 203 ms, -3986 dropped it to
    // 17 ms — override semantics would have given 1 s / 2 s / 100 ms;
    // offsets are also not clamped — see HANDOVER.md). So the requested
    // absolute time is converted to the offset that moves the font's own
    // base value to the desired time. Channel-gen offsets are not clamped
    // to the SF2 spec range, so the full 1..2000 / 1..4000 ms config range
    // is reachable. Sustain is left alone: the font already holds it at
    // full level (0 cB attenuation).
    const float attack_tc =
        static_cast<float>(ms_to_timecents(attack_ms_) - kSf2AttackTc);
    const float release_tc =
        static_cast<float>(ms_to_timecents(release_ms_) - kSf2ReleaseTc);
    int midi_channels = fluid_synth_count_midi_channels(synth_);
    for (int ch = 0; ch < midi_channels; ++ch) {
        fluid_synth_set_gen(synth_, ch, GEN_VOLENVATTACK, attack_tc);
        fluid_synth_set_gen(synth_, ch, GEN_VOLENVRELEASE, release_tc);
    }
}

int HarmoniumPlugin::stop_preset_index(const std::string& stop) {
    // Preset layout of the derived font (plugins/harmonium/soundfonts/
    // harmonium_v2.sf2, tests/scripts/derive_sf2.py): preset 0 "harmonium"
    // = single reed, preset 1 "harmonium double" = detuned unison pair.
    if (stop == "single") {
        return 0;
    }
    if (stop == "double") {
        return 1;
    }
    return -1;
}

void HarmoniumPlugin::apply_stop() {
    if (!synth_ || soundfont_id_ < 0) {
        return;
    }
    const int preset = stop_preset_index(stop_);
    const int midi_channels = fluid_synth_count_midi_channels(synth_);
    for (int ch = 0; ch < midi_channels; ++ch) {
        fluid_synth_program_select(synth_, ch, soundfont_id_, 0, preset);
    }
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

std::vector<HarmoniumPlugin::HeldNote>::iterator HarmoniumPlugin::find_held(uint8_t note) {
    return std::find_if(held_notes_.begin(), held_notes_.end(),
                        [note](const HeldNote& held) { return held.note == note; });
}

void HarmoniumPlugin::release_held_note(uint8_t note) {
    auto it = find_held(note);
    if (it != held_notes_.end()) {
        held_notes_.erase(it);
    }
    if (held_notes_.empty()) {
        reference_velocity_ = 0;
    } else {
        reference_velocity_ = held_notes_.front().velocity;
    }
}

PluginResult HarmoniumPlugin::handle_midi_event(const MidiEvent& event) {
    if (!synth_) {
        return PLUGIN_ERROR;
    }

    switch (event.type) {
        case MidiEvent::NOTE_ON: {
            if (event.data2 > 0) {
                // Uniform bellows velocity: the first key of a sequence
                // latches the reference; every key pressed while the
                // bellows is open (notes held) sounds at it. Releasing
                // the reference key passes the baton to the next oldest
                // held key's original press velocity.
                if (held_notes_.empty()) {
                    reference_velocity_ = event.data2;
                }
                uint8_t effective_velocity = reference_velocity_;

                if (find_held(event.data1) == held_notes_.end()) {
                    held_notes_.push_back({event.data1, event.data2});
                }

                // Note on with velocity
                int result = fluid_synth_noteon(synth_, event.channel, event.data1, effective_velocity);
                if (result != 0) {
                    std::cerr << "Note on failed: ch=" << (int)event.channel
                              << " note=" << (int)event.data1 << std::endl;
                    return PLUGIN_ERROR;
                }
            } else {
                // Note on with 0 velocity is note off
                release_held_note(event.data1);
                fluid_synth_noteoff(synth_, event.channel, event.data1);
            }
            break;
        }

        case MidiEvent::NOTE_OFF: {
            release_held_note(event.data1);
            fluid_synth_noteoff(synth_, event.channel, event.data1);
            break;
        }
        
        case MidiEvent::CONTROL_CHANGE: {
            // All Notes Off: reset bellows state (stuck-note insurance)
            if (event.data1 == 123) {
                held_notes_.clear();
                reference_velocity_ = 0;
            }
            fluid_synth_cc(synth_, event.channel, event.data1, event.data2);
            break;
        }
        
        case MidiEvent::PITCH_BEND: {
            int value = event.data1 | (event.data2 << 7);
            fluid_synth_pitch_bend(synth_, event.channel, value);
            break;
        }
        
        case MidiEvent::PROGRAM_CHANGE: {
            // Deliberately IGNORED (Phase 3 policy): stops are
            // config-controlled ("stop" key), and forwarding program
            // changes to FluidSynth would silently switch reed stops — a
            // stray program change would wreck the voicing. Future idea:
            // map program changes to MIDI-controlled stop switches.
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
    if (k == "gain") {
        if (synth_) {
            gain_ = fluid_synth_get_gain(synth_);
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3f", gain_);
        return buf;
    }
    if (k == "reverb") {
        return reverb_on_ ? "on" : "off";
    }
    if (k == "chorus") {
        return chorus_on_ ? "on" : "off";
    }
    if (k == "attack_ms") {
        return std::to_string(attack_ms_);
    }
    if (k == "release_ms") {
        return std::to_string(release_ms_);
    }
    if (k == "stop") {
        return stop_;
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
    if (k == "gain") {
        std::string v = value;
        char* end = nullptr;
        float gain = std::strtof(v.c_str(), &end);
        if (end == v.c_str() || *end != '\0' || !(gain >= 0.0f && gain <= 10.0f)) {
            return PLUGIN_INVALID_PARAM;
        }
        gain_ = gain;
        if (synth_) {
            fluid_synth_set_gain(synth_, gain_);
        }
        return PLUGIN_OK;
    }
    if (k == "reverb") {
        std::string v = value;
        if (v == "on") {
            reverb_on_ = true;
        } else if (v == "off") {
            reverb_on_ = false;
        } else {
            return PLUGIN_INVALID_PARAM;
        }
        if (synth_) {
            fluid_synth_reverb_on(synth_, -1, reverb_on_ ? 1 : 0);
        }
        return PLUGIN_OK;
    }
    if (k == "chorus") {
        std::string v = value;
        if (v == "on") {
            chorus_on_ = true;
        } else if (v == "off") {
            chorus_on_ = false;
        } else {
            return PLUGIN_INVALID_PARAM;
        }
        if (synth_) {
            fluid_synth_chorus_on(synth_, -1, chorus_on_ ? 1 : 0);
        }
        return PLUGIN_OK;
    }
    if (k == "attack_ms") {
        std::string v = value;
        char* end = nullptr;
        long ms = std::strtol(v.c_str(), &end, 10);
        if (end == v.c_str() || *end != '\0' || !(ms >= 1 && ms <= 2000)) {
            return PLUGIN_INVALID_PARAM;
        }
        attack_ms_ = static_cast<int>(ms);
        if (synth_) {
            apply_envelope_gens();
        }
        return PLUGIN_OK;
    }
    if (k == "release_ms") {
        std::string v = value;
        char* end = nullptr;
        long ms = std::strtol(v.c_str(), &end, 10);
        if (end == v.c_str() || *end != '\0' || !(ms >= 1 && ms <= 4000)) {
            return PLUGIN_INVALID_PARAM;
        }
        release_ms_ = static_cast<int>(ms);
        if (synth_) {
            apply_envelope_gens();
        }
        return PLUGIN_OK;
    }

    if (k == "stop") {
        const std::string v = value;
        if (stop_preset_index(v) < 0) {
            return PLUGIN_INVALID_PARAM;
        }
        stop_ = v;
        // Apply immediately when the synth + font are ready; otherwise
        // init() applies the stored stop right after sfload.
        if (synth_ && soundfont_id_ >= 0) {
            apply_stop();
        }
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
