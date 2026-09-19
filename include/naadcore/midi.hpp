#ifndef NAADCORE_MIDI_HPP
#define NAADCORE_MIDI_HPP

#include <alsa/asoundlib.h>
#include <fluidsynth.h>
#include <memory>
#include <functional>
#include <string>

namespace naadcore {

/**
 * @brief MIDI input handler using ALSA sequencer
 * 
 * Reads MIDI events from an ALSA sequencer client:port
 * and forwards them to FluidSynth.
 */
class MidiInput {
public:
    using EventCallback = std::function<void(const snd_seq_event_t&)>;

    MidiInput();
    ~MidiInput();

    /**
     * @brief Open ALSA sequencer connection
     * @param client_port String in format "client:port" (e.g., "20:0")
     * @return true if successful, false otherwise
     */
    bool open(const std::string& client_port);

    /**
     * @brief Close the sequencer connection
     */
    void close();

    /**
     * @brief Set callback for MIDI events
     * @param callback Function to call when MIDI event received
     */
    void set_callback(EventCallback callback);

    /**
     * @brief Poll and process incoming MIDI events
     * @return Number of events processed
     */
    int process_events();

    /**
     * @brief Get the file descriptor for select() polling
     * @return File descriptor, or -1 if not open
     */
    int get_fd() const;

private:
    snd_seq_t* seq_;
    EventCallback callback_;
    int client_;
    int port_;
    int fd_; ///< ALSA sequencer file descriptor for select()
};

/**
 * @brief FluidSynth synthesizer wrapper
 * 
 * Manages FluidSynth instance, SoundFont loading,
 * and provides high-level note on/off controls.
 */
class Synthesizer {
public:
    Synthesizer();
    ~Synthesizer();

    /**
     * @brief Initialize FluidSynth with default settings
     * @return true if successful, false otherwise
     */
    bool init();

    /**
     * @brief Load a SoundFont file
     * @param filename Path to .sf2 file
     * @param reset_presets Whether to reset presets after loading
     * @return SoundFont ID on success, -1 on failure
     */
    int load_soundfont(const std::string& filename, bool reset_presets = true);

    /**
     * @brief Start audio driver
     * @return true if successful, false otherwise
     */
    bool start_audio();

    /**
     * @brief Stop audio driver
     */
    void stop_audio();

    /**
     * @brief Set the FluidSynth audio driver to use
     * @param driver Driver name ("alsa", "pipewire", "pulseaudio", ...)
     *               Must be called before start_audio()
     */
    void set_audio_driver(const std::string& driver) { audio_driver_ = driver; }

    /**
     * @brief Send note on event
     * @param channel MIDI channel (0-15)
     * @param note Note number (0-127)
     * @param velocity Velocity (0-127)
     * @return 0 on success, negative on error
     */
    int note_on(int channel, int note, int velocity);

    /**
     * @brief Send note off event
     * @param channel MIDI channel (0-15)
     * @param note Note number (0-127)
     * @return 0 on success, negative on error
     */
    int note_off(int channel, int note);

    /**
     * @brief Get FluidSynth settings pointer
     * @return Settings pointer
     */
    fluid_settings_t* settings() { return settings_; }

    /**
     * @brief Get FluidSynth synth pointer
     * @return Synth pointer
     */
    fluid_synth_t* synth() { return synth_; }

private:
    fluid_settings_t* settings_;
    fluid_synth_t* synth_;
    fluid_audio_driver_t* driver_;
    std::string audio_driver_;
};

} // namespace naadcore

#endif // NAADCORE_MIDI_HPP
