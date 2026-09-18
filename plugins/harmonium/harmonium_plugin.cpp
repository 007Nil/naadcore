#include "harmonium_plugin.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cerrno>
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

    // Key click (Phase 6): channel 12 must NOT play the stop preset —
    // apply_stop() looped all channels, so re-select the click preset (2)
    // on it BEFORE asserting the channel gains (a FAILED program_select —
    // e.g. a font without preset 2 like harmonium_v2.sf2 — resets the
    // channel's CC 7 to full, so the fixed click gain must be re-asserted
    // afterwards by apply_layer_gains(); with v3 the selection succeeds
    // and the order is simply harmless).
    apply_click_preset();
    apply_layer_gains();
    apply_coupler_detune();
    std::cout << "Synth layers: coupler=" << (coupler_on_ ? "on" : "off")
              << " sub_octave=" << (sub_octave_on_ ? "on" : "off")
              << " (ch" << kCouplerChannel << "=note+12 CC7=" << kCouplerCC7
              << ", ch" << kSubOctaveChannel << "=note-12 CC7="
              << kSubOctaveCC7 << ")" << std::endl;

    std::cout << "Synth click: key_click=" << key_click_
              << " variation=" << (variation_on_ ? "on" : "off")
              << " (ch" << kClickChannel << " preset " << kClickPreset
              << " CC7=" << kClickCC7
              << ", vel low/high=" << static_cast<int>(kClickVelLow)
              << "/" << static_cast<int>(kClickVelHigh)
              << ", jitter main +-1.." << kJitterMain
              << " click +-1.." << kJitterClick
              << ", seed " << kVariationSeed << ")" << std::endl;

    // Drone (Phase 5): channel 13 carries the drone fixture on the same
    // stop preset (apply_stop() above already programmed it). Set its
    // gain first, then start the configured drone notes (they sound
    // until the spec changes or CC 123 clears them).
    apply_drone_level();
    apply_drone_spec();
    std::cout << "Synth drone: " << drone_spec_
              << " (ch" << kDroneChannel << " CC7=" << drone_level_
              << " vel=" << static_cast<int>(kDroneVelocity) << ")"
              << std::endl;

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

void HarmoniumPlugin::apply_layer_gains() {
    if (!synth_) {
        return;
    }
    // The internal channels carry their layer at a fixed gain below the
    // main voice; CC 7 events are never mirrored to them (see
    // handle_midi_event), so these values stay the layer gain knob.
    fluid_synth_cc(synth_, kCouplerChannel, 7, kCouplerCC7);
    fluid_synth_cc(synth_, kSubOctaveChannel, 7, kSubOctaveCC7);
    // The click layer's fixed channel gain lives here too: apply_layer_
    // gains() is the "re-assert all internal-channel gains" helper called
    // at init and after every stop change.
    fluid_synth_cc(synth_, kClickChannel, 7,
                   static_cast<uint8_t>(kClickCC7));
}

void HarmoniumPlugin::apply_coupler_detune() {
    if (!synth_) {
        return;
    }
    // Optional subtle beat between the main voice and its octave coupler:
    // raise the coupler channel +3 cents via the MIDI Tuning Standard API.
    // Tunings live outside the SoundFont/preset namespace and survive
    // program_select, so this is applied once at init. Failure is silent —
    // the double-stop zones already provide shimmer (this is a bonus).
    std::vector<double> pitch(128, kCouplerDetuneCents);
    if (fluid_synth_activate_key_tuning(synth_, 0, 0, "coupler+3c",
                                        pitch.data(), 0) == 0) {
        fluid_synth_activate_tuning(synth_, kCouplerChannel, 0, 0, 0);
    }
}

bool HarmoniumPlugin::parse_drone_spec(const std::string& value,
                                       std::vector<uint8_t>& out) {
    out.clear();
    if (value == "off" || value.empty()) {
        return true;  // empty string = off (canonicalized by the caller)
    }
    // Comma-separated note numbers, strict: digits only (no whitespace,
    // signs or floats — same strictness as the other config keys), each
    // 0-127, no duplicates, at most kDroneMaxNotes notes. Sargam-name
    // parsing ("Sa", "Pa") is future work.
    size_t pos = 0;
    while (true) {
        const size_t comma = value.find(',', pos);
        const std::string token =
            value.substr(pos, comma == std::string::npos
                                  ? std::string::npos : comma - pos);
        if (token.empty() ||
            token.find_first_not_of("0123456789") != std::string::npos) {
            return false;
        }
        errno = 0;
        char* end = nullptr;
        const unsigned long n = std::strtoul(token.c_str(), &end, 10);
        if (errno != 0 || end != token.c_str() + token.size() || n > 127) {
            return false;
        }
        if (out.size() >= kDroneMaxNotes ||
            std::find(out.begin(), out.end(),
                      static_cast<uint8_t>(n)) != out.end()) {
            return false;  // more than kDroneMaxNotes notes, or duplicate
        }
        out.push_back(static_cast<uint8_t>(n));
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
    return true;
}

void HarmoniumPlugin::start_drone_note(uint8_t note) {
    if (!synth_) {
        return;
    }
    if (fluid_synth_noteon(synth_, kDroneChannel, note, kDroneVelocity) != 0) {
        // No font zone for this note (or voice overflow): don't record
        // it, so the release path stays a no-op — same policy as the
        // layer router's failed noteons.
        std::cerr << "Drone note failed to start: note="
                  << static_cast<int>(note) << std::endl;
        return;
    }
    drone_notes_.push_back(note);
}

void HarmoniumPlugin::apply_drone_spec() {
    if (!synth_ || soundfont_id_ < 0) {
        return;
    }
    std::vector<uint8_t> wanted;
    if (!parse_drone_spec(drone_spec_, wanted)) {
        return;  // defensive: the stored spec is always valid
    }
    // Release sounding notes that are no longer wanted (fluid_synth_
    // noteoff → natural release_ms tail); start wanted notes that are
    // not sounding; unchanged notes are left untouched (no re-trigger).
    for (auto it = drone_notes_.begin(); it != drone_notes_.end();) {
        if (std::find(wanted.begin(), wanted.end(), *it) == wanted.end()) {
            fluid_synth_noteoff(synth_, kDroneChannel, *it);
            it = drone_notes_.erase(it);
        } else {
            ++it;
        }
    }
    for (const uint8_t note : wanted) {
        if (std::find(drone_notes_.begin(), drone_notes_.end(), note)
                == drone_notes_.end()) {
            start_drone_note(note);
        }
    }
}

void HarmoniumPlugin::apply_drone_level() {
    if (!synth_) {
        return;
    }
    // CC 7 on channel 13 IS the drone's gain knob (never mirrored from
    // MIDI input, like the layer gains on channels 14/15).
    fluid_synth_cc(synth_, kDroneChannel, 7,
                   static_cast<uint8_t>(drone_level_));
}

void HarmoniumPlugin::apply_click_preset() {
    if (!synth_ || soundfont_id_ < 0) {
        return;
    }
    // The click layer plays the font's preset 2 ("key click" — the
    // self-ending click instrument), NOT the stop preset. Must run after
    // every apply_stop(), which re-programmes ALL channels (including 12),
    // and BEFORE the channel gains are re-asserted (a FAILED selection
    // resets the channel's CC 7 — see init()). With a font lacking preset
    // 2 (harmonium_v2.sf2) the selection fails and the flag below stays
    // false: the click layer then stays a SILENT no-op (without this
    // guard, a noteon on the channel would fall back to the stop preset
    // and stack a quiet duplicate reed voice on every note).
    click_preset_ok_ =
        (fluid_synth_program_select(synth_, kClickChannel, soundfont_id_, 0,
                                    kClickPreset) == 0);
}

uint8_t HarmoniumPlugin::jitter_velocity(int velocity, int max_jitter,
                                         int& last_jitter) {
    // Random magnitude 1..max_jitter with a random sign — never zero, so
    // a varied note never plays at the exact input velocity (defeats
    // sample-identical repeats). Anti-repeat: redraw (bounded) while the
    // draw equals the previous one — two consecutive varied notes never
    // coincide (the honest caveat: at the velocity clamp edges, 1 or 127,
    // two different jitter draws can still clamp to the same velocity).
    // Deterministic: fixed-seed mt19937, so identical event sequences
    // produce identical velocity patterns.
    std::uniform_int_distribution<int> magnitude(1, max_jitter);
    std::uniform_int_distribution<int> sign(0, 1);
    int j = 0;
    for (int attempt = 0; attempt < 4; ++attempt) {
        j = magnitude(rng_) * (sign(rng_) ? 1 : -1);
        if (j != last_jitter) {
            break;
        }
    }
    last_jitter = j;
    const int v = std::clamp(velocity + j, 1, 127);
    return static_cast<uint8_t>(v);
}

void HarmoniumPlugin::trigger_click(uint8_t note) {
    if (!synth_ || soundfont_id_ < 0 || key_click_ == "off"
        || !click_preset_ok_) {
        return;
    }
    const int base = (key_click_ == "high") ? kClickVelHigh : kClickVelLow;
    const int vel = variation_on_
                        ? jitter_velocity(base, kJitterClick,
                                          last_click_jitter_)
                        : base;
    // Fire-and-forget: the click instrument's envelope is self-ending
    // (decay 40 ms to a fully-closed sustain; proven by render to end the
    // voice <= 60 ms — tests/RESULTS.md Phase 6), so NO noteoff tracking,
    // NOT in held_notes_, invisible to the bellows model. A note outside
    // the click zone (keys 21-108) simply produces no voice — silent skip.
    (void)fluid_synth_noteon(synth_, kClickChannel, note, vel);
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

bool HarmoniumPlugin::release_held_note(uint8_t note) {
    auto it = find_held(note);
    if (it == held_notes_.end()) {
        return false;
    }

    // Release ALL voices started for this note: the layer voices on the
    // fixed internal channels and the main voice on the channel the note
    // originally arrived on (a cross-channel NoteOff must not strand a
    // voice — one harmonium, one bellows; the stored channel is only a
    // FluidSynth-routing detail).
    if (it->layers & kLayerCoupler) {
        fluid_synth_noteoff(synth_, kCouplerChannel, it->note + 12);
    }
    if (it->layers & kLayerSubOctave) {
        fluid_synth_noteoff(synth_, kSubOctaveChannel, it->note - 12);
    }
    fluid_synth_noteoff(synth_, it->channel, it->note);

    held_notes_.erase(it);
    if (held_notes_.empty()) {
        reference_velocity_ = 0;
    } else {
        reference_velocity_ = held_notes_.front().velocity;
    }
    return true;
}

void HarmoniumPlugin::set_note_layer(HeldNote& held, uint8_t layer_bit, bool on) {
    const bool coupler = (layer_bit == kLayerCoupler);
    const int channel = coupler ? kCouplerChannel : kSubOctaveChannel;
    const int layer_note = coupler ? held.note + 12 : held.note - 12;

    // Range clamps: a coupler voice above MIDI note 127 or a sub-octave
    // voice below 0 cannot exist — silently skip (no logging).
    if (layer_note < 0 || layer_note > 127) {
        return;
    }
    if (on) {
        // Mid-phrase layer toggles replay the note's stored PLAYED
        // (jittered) velocity — the same pressure this key press produced,
        // not a fresh random draw.
        if (fluid_synth_noteon(synth_, channel,
                               static_cast<uint8_t>(layer_note),
                               held.played_velocity) == 0) {
            held.layers |= layer_bit;
        }
        // A failed noteon (e.g. key outside the font's zones) simply
        // leaves the bit unset — the release path then stays a no-op.
    } else {
        if (held.layers & layer_bit) {
            fluid_synth_noteoff(synth_, channel,
                                static_cast<uint8_t>(layer_note));
        }
        held.layers &= static_cast<uint8_t>(~layer_bit);
    }
}

void HarmoniumPlugin::set_layer_for_all_held(uint8_t layer_bit, bool on) {
    for (HeldNote& held : held_notes_) {
        set_note_layer(held, layer_bit, on);
    }
}

PluginResult HarmoniumPlugin::handle_midi_event(const MidiEvent& event) {
    if (!synth_) {
        return PLUGIN_ERROR;
    }

    switch (event.type) {
        case MidiEvent::NOTE_ON: {
            if (event.data2 > 0) {
                // Duplicate NoteOn for an already-held key: IGNORED. The
                // pallet is already open and sounding; re-triggering would
                // re-attack the reed mid-phrase (audible Phase <=3 bug,
                // fixed in Phase 4). Nothing is updated and no FluidSynth
                // call is made.
                if (find_held(event.data1) != held_notes_.end()) {
                    break;
                }

                // Uniform bellows velocity: the first key of a sequence
                // latches the reference; every key pressed while the
                // bellows is open (notes held) sounds at it. Releasing
                // the reference key passes the baton to the next oldest
                // held key's original press velocity.
                if (held_notes_.empty()) {
                    reference_velocity_ = event.data2;
                }
                const uint8_t sounding_velocity = reference_velocity_;

                // Per-note micro-variation (Phase 6): jitter ONLY the
                // velocity handed to FluidSynth — the bellows reference
                // latch/baton bookkeeping above stays exact (raw press
                // velocities in reference_velocity_ / HeldNote.velocity).
                // +-1..3 is imperceptible dynamically but the main and
                // layer voices of repeated keys are no longer
                // sample-identical. With variation off: exact velocities.
                const uint8_t played_velocity =
                    variation_on_
                        ? jitter_velocity(sounding_velocity, kJitterMain,
                                          last_main_jitter_)
                        : sounding_velocity;

                // Note on with velocity
                int result = fluid_synth_noteon(synth_, event.channel, event.data1, played_velocity);
                if (result != 0) {
                    std::cerr << "Note on failed: ch=" << (int)event.channel
                              << " note=" << (int)event.data1 << std::endl;
                    return PLUGIN_ERROR;
                }

                // Layer router (Phase 4): start the octave-coupler and
                // sub-octave voices for this note, all at the played
                // (jittered) bellows velocity of this key press (one
                // bellows, one finger noise per press — layers never
                // fork it).
                HeldNote held{event.data1, event.data2, event.channel,
                              sounding_velocity, played_velocity, 0};
                if (coupler_on_) {
                    set_note_layer(held, kLayerCoupler, true);
                }
                if (sub_octave_on_) {
                    set_note_layer(held, kLayerSubOctave, true);
                }
                held_notes_.push_back(held);

                // Key click (Phase 6): a faint mechanical transient as the
                // pallet opens — only on ACCEPTED NoteOns. Drone changes
                // never reach this path, and a swallowed duplicate NoteOn
                // (handled above) correctly makes no click: no pallet
                // moved. The voice self-ends via its preset's envelope —
                // no noteoff tracking (see trigger_click).
                trigger_click(event.data1);
            } else {
                // Note on with 0 velocity is note off
                if (!release_held_note(event.data1)) {
                    // Not held here: pass the release through unchanged.
                    fluid_synth_noteoff(synth_, event.channel, event.data1);
                }
            }
            break;
        }

        case MidiEvent::NOTE_OFF: {
            if (!release_held_note(event.data1)) {
                // Not held here: pass the release through unchanged.
                fluid_synth_noteoff(synth_, event.channel, event.data1);
            }
            break;
        }

        case MidiEvent::CONTROL_CHANGE: {
            // All Notes Off: reset bellows state (stuck-note insurance)
            // and silence ALL 16 channels — CC 123 sent to one channel
            // only clears that channel's voices, and layer/drone/click
            // voices live on internal channels the sender knows nothing
            // about (15 coupler, 14 sub, 13 drone, 12 click — a sounding
            // click voice is simply cut short here; there is no click
            // state to clear, it is not tracked).
            // CC 123 is a FULL reset: the drone's sounding-note container
            // is cleared too (its all_notes_off below covers channel 13),
            // while the stored "drone" spec is kept — re-issuing the same
            // value restarts the notes (the start/stop diff runs against
            // the sounding state, see apply_drone_spec).
            if (event.data1 == 123) {
                held_notes_.clear();
                reference_velocity_ = 0;
                drone_notes_.clear();
                const int midi_channels = fluid_synth_count_midi_channels(synth_);
                for (int ch = 0; ch < midi_channels; ++ch) {
                    fluid_synth_all_notes_off(synth_, ch);
                }
            }
            fluid_synth_cc(synth_, event.channel, event.data1, event.data2);
            // Mirror expression (CC 11) to active layers so they track the
            // main voice. CC 7 (volume) is deliberately NOT mirrored: on
            // the internal channels it IS their fixed gain knob
            // (kCouplerCC7 / kSubOctaveCC7). Pitch bend is mirrored in its
            // own case below.
            if (event.data1 == 11) {
                if (coupler_on_) {
                    fluid_synth_cc(synth_, kCouplerChannel, 11, event.data2);
                }
                if (sub_octave_on_) {
                    fluid_synth_cc(synth_, kSubOctaveChannel, 11, event.data2);
                }
            }
            break;
        }

        case MidiEvent::PITCH_BEND: {
            int value = event.data1 | (event.data2 << 7);
            fluid_synth_pitch_bend(synth_, event.channel, value);
            if (coupler_on_) {
                fluid_synth_pitch_bend(synth_, kCouplerChannel, value);
            }
            if (sub_octave_on_) {
                fluid_synth_pitch_bend(synth_, kSubOctaveChannel, value);
            }
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
    if (k == "coupler") {
        return coupler_on_ ? "on" : "off";
    }
    if (k == "sub_octave") {
        return sub_octave_on_ ? "on" : "off";
    }
    if (k == "drone") {
        return drone_spec_;
    }
    if (k == "drone_level") {
        return std::to_string(drone_level_);
    }
    if (k == "key_click") {
        return key_click_;
    }
    if (k == "variation") {
        return variation_on_ ? "on" : "off";
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
        // init() applies the stored stop right after sfload. The layer
        // channels 14/15 are re-programmed by apply_stop() itself (it
        // loops all channels); re-assert their gains in case anything
        // reset them along with the preset.
        if (synth_ && soundfont_id_ >= 0) {
            // Order matters: apply_stop() re-programmes ALL channels
            // (including the click channel); the click preset selection
            // must run before the gains are re-asserted (a FAILED
            // selection resets the channel's CC 7 — see init()).
            apply_stop();
            apply_click_preset();
            apply_layer_gains();
            // The drone channel plays the stop preset too (apply_stop
            // loops all channels); re-assert its gain alongside the
            // layer gains in case anything reset it with the preset.
            apply_drone_level();
        }
        return PLUGIN_OK;
    }

    // Layer router (Phase 4): octave coupler + sub-octave toggles. Like
    // the other on/off keys, values are strictly validated. Toggling while
    // notes are held starts/releases that layer for every held note at
    // its stored sounding_velocity (mid-phrase coupler change, like the
    // predecessor's refreshAudio); the bellows reference is untouched.
    if (k == "coupler" || k == "sub_octave") {
        const std::string v = value;
        bool on = false;
        if (v == "on") {
            on = true;
        } else if (v == "off") {
            on = false;
        } else {
            return PLUGIN_INVALID_PARAM;
        }
        const bool is_coupler = (k == "coupler");
        bool& flag = is_coupler ? coupler_on_ : sub_octave_on_;
        const bool changed = (flag != on);
        flag = on;
        if (synth_ && changed) {
            set_layer_for_all_held(is_coupler ? kLayerCoupler
                                              : kLayerSubOctave,
                                   on);
        }
        return PLUGIN_OK;
    }

    // Drone (Phase 5): fixture notes on internal channel 13. Live
    // semantics: newly added notes start immediately (fixed drone
    // velocity — loudness is the drone_level CC 7 gain), removed notes
    // release with the natural release_ms tail, unchanged notes keep
    // sounding without re-trigger. Drone voices never touch the bellows
    // model (no held_notes_ / reference_velocity_ interaction).
    if (k == "drone") {
        std::vector<uint8_t> wanted;
        if (!parse_drone_spec(value, wanted)) {
            return PLUGIN_INVALID_PARAM;
        }
        // Canonical echo: "" normalizes to "off".
        drone_spec_ = wanted.empty() ? "off" : value;
        if (synth_ && soundfont_id_ >= 0) {
            apply_drone_spec();
        }
        return PLUGIN_OK;
    }

    if (k == "drone_level") {
        std::string v = value;
        char* end = nullptr;
        long level = std::strtol(v.c_str(), &end, 10);
        if (end == v.c_str() || *end != '\0' || !(level >= 0 && level <= 127)) {
            return PLUGIN_INVALID_PARAM;
        }
        drone_level_ = static_cast<int>(level);
        if (synth_) {
            apply_drone_level();
        }
        return PLUGIN_OK;
    }

    // Key click (Phase 6): "off" (default) | "low" | "high". Stored state
    // only — the mode is consulted per accepted NoteOn (trigger_click),
    // so pre-init storage applies at init and a live toggle applies from
    // the NEXT accepted NoteOn (held notes keep sounding unchanged; their
    // clicks already fired).
    if (k == "key_click") {
        const std::string v = value;
        if (v != "off" && v != "low" && v != "high") {
            return PLUGIN_INVALID_PARAM;
        }
        key_click_ = v;
        return PLUGIN_OK;
    }

    // Per-note micro-variation (Phase 6): on (default) | off. Like
    // key_click, consulted per accepted NoteOn (jitter_velocity uses the
    // fixed-seed PRNG — deterministic for identical event sequences).
    if (k == "variation") {
        const std::string v = value;
        if (v == "on") {
            variation_on_ = true;
        } else if (v == "off") {
            variation_on_ = false;
        } else {
            return PLUGIN_INVALID_PARAM;
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
