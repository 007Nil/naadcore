#include "naadcore/midi.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace naadcore {

MidiInput::MidiInput() : seq_(nullptr), client_(0), port_(0), fd_(-1) {
}

MidiInput::~MidiInput() {
    close();
}

bool MidiInput::open(const std::string& client_port) {
    // Parse client:port format
    size_t colon_pos = client_port.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid client:port format: " << client_port << std::endl;
        return false;
    }

    client_ = std::atoi(client_port.substr(0, colon_pos).c_str());
    port_ = std::atoi(client_port.substr(colon_pos + 1).c_str());

    // Open sequencer in input mode, non-blocking (main loop polls)
    int err = snd_seq_open(&seq_, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
    if (err < 0) {
        std::cerr << "Failed to open ALSA sequencer: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set client name
    snd_seq_set_client_name(seq_, "naadcore");

    // Create input port
    snd_seq_port_info_t* port_info;
    snd_seq_port_info_alloca(&port_info);
    snd_seq_port_info_set_port(port_info, 0);  // Port 0
    snd_seq_port_info_set_port_specified(port_info, 1);
    
    // Configure port capabilities for RECEIVING MIDI: other ports write into
    // this port. (READ caps would make it an event source instead, which is
    // why events were never delivered to us even though the subscription
    // appeared in aconnect -l.)
    snd_seq_port_info_set_capability(port_info, 
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE);
    snd_seq_port_info_set_type(port_info, 
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    snd_seq_port_info_set_midi_channels(port_info, 16);
    snd_seq_port_info_set_name(port_info, "naadcore input");
    
    int p = snd_seq_create_port(seq_, port_info);
    if (p < 0) {
        std::cerr << "Failed to create sequencer port: " << snd_strerror(p) << std::endl;
        snd_seq_close(seq_);
        seq_ = nullptr;
        return false;
    }

    // Subscribe to the source client:port
    snd_seq_port_subscribe_t* sub;
    snd_seq_port_subscribe_alloca(&sub);
    
    // Source (sender) - the parsed client:port (e.g. the Q49)
    snd_seq_addr_t source = { static_cast<unsigned char>(client_), 
                              static_cast<unsigned char>(port_) };
    snd_seq_port_subscribe_set_sender(sub, &source);
    
    // Destination (receiver) - our port (cast is safe since client IDs are 0-255)
    unsigned char my_client = static_cast<unsigned char>(snd_seq_client_id(seq_));
    snd_seq_addr_t dest = { my_client, static_cast<unsigned char>(p) };
    snd_seq_port_subscribe_set_dest(sub, &dest);
    
    err = snd_seq_subscribe_port(seq_, sub);
    if (err < 0) {
        std::cerr << "Failed to subscribe to port " << client_port << ": " 
                  << snd_strerror(err) << std::endl;
        snd_seq_close(seq_);
        seq_ = nullptr;
        return false;
    }

    std::cout << "MIDI input connected: " << client_port << " -> "
              << (int)my_client << ":" << p << std::endl;
    
    // Get the file descriptor for select() - the sequencer uses poll() internally
    // but has a single file descriptor we can use for select()
    struct pollfd pfd;
    int err2 = snd_seq_poll_descriptors(seq_, &pfd, 1, POLLIN);
    if (err2 >= 1) {
        fd_ = pfd.fd;
    } else {
        fd_ = -1;
    }
    
    return true;
}

void MidiInput::close() {
    if (seq_) {
        snd_seq_close(seq_);
        seq_ = nullptr;
    }
    fd_ = -1;
}

void MidiInput::set_callback(EventCallback callback) {
    callback_ = std::move(callback);
}

int MidiInput::process_events() {
    if (!seq_) {
        return 0;
    }

    int count = 0;

    // Drain all pending events (non-blocking; returns -EAGAIN when empty)
    snd_seq_event_t* ev = nullptr;
    while (snd_seq_event_input(seq_, &ev) >= 0 && ev != nullptr) {
        if (callback_) {
            callback_(*ev);
        }
        count++;
        ev = nullptr;
    }

    return count;
}

int MidiInput::get_fd() const {
    return fd_;
}

// ============================================================================
// Synthesizer implementation
// ============================================================================

Synthesizer::Synthesizer() 
    : settings_(nullptr), synth_(nullptr), driver_(nullptr), audio_driver_("alsa") {
}

Synthesizer::~Synthesizer() {
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

bool Synthesizer::init() {
    // Create settings
    settings_ = new_fluid_settings();
    if (!settings_) {
        std::cerr << "Failed to create FluidSynth settings" << std::endl;
        return false;
    }

    // Create synth
    synth_ = new_fluid_synth(settings_);
    if (!synth_) {
        std::cerr << "Failed to create FluidSynth instance" << std::endl;
        return false;
    }

    // Set audio driver type to 'file' initially (we'll use the default driver)
    // The default driver will be auto-selected based on system capabilities
    return true;
}

int Synthesizer::load_soundfont(const std::string& filename, bool reset_presets) {
    int id = fluid_synth_sfload(synth_, filename.c_str(), reset_presets);
    if (id < 0) {
        std::cerr << "Failed to load SoundFont: " << filename << std::endl;
    } else {
        std::cout << "Loaded SoundFont: " << filename << " (ID: " << id << ")" << std::endl;
    }
    return id;
}

bool Synthesizer::start_audio() {
    if (!synth_) {
        std::cerr << "Synth not initialized" << std::endl;
        return false;
    }

    // Use an explicit audio driver. The auto-selected default can be broken
    // on some systems (e.g. an SDL3 build that is not initialized), which
    // results in silence. "alsa" is proven to work on this machine;
    // "pipewire" and "pulseaudio" are also available as fallbacks.
    if (!audio_driver_.empty()) {
        fluid_settings_setstr(settings_, "audio.driver", audio_driver_.c_str());
    }

    driver_ = new_fluid_audio_driver(settings_, synth_);
    if (!driver_) {
        std::cerr << "Failed to create audio driver '" << audio_driver_ << "' "
                     "(try --audio-driver pipewire or pulseaudio)" << std::endl;
        return false;
    }

    std::cout << "Audio driver started: "
              << (audio_driver_.empty() ? "(default)" : audio_driver_) << std::endl;
    return true;
}

void Synthesizer::stop_audio() {
    if (driver_) {
        delete_fluid_audio_driver(driver_);
        driver_ = nullptr;
    }
}

int Synthesizer::note_on(int channel, int note, int velocity) {
    if (!synth_) {
        return -1;
    }
    return fluid_synth_noteon(synth_, channel, note, velocity);
}

int Synthesizer::note_off(int channel, int note) {
    if (!synth_) {
        return -1;
    }
    return fluid_synth_noteoff(synth_, channel, note);
}

} // namespace naadcore
