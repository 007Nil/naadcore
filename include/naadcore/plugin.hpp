#ifndef NAADCORE_PLUGIN_HPP
#define NAADCORE_PLUGIN_HPP

#include <cstdint>
#include <string>

/**
 * @file plugin.hpp
 * @brief Plugin interface for NaadCore
 * 
 * Plugins are dynamically loadable libraries that implement
 * the INaadPlugin interface. Each plugin is responsible for
 * its own SoundFont loading and synthesis.
 */

namespace naadcore {

/**
 * @brief MIDI event structure for plugin interface
 * 
 * This structure is used to pass MIDI events to plugins
 * in a format independent of ALSA or FluidSynth internals.
 */
struct MidiEvent {
    enum Type {
        NOTE_ON = 1,
        NOTE_OFF = 2,
        CONTROL_CHANGE = 3,
        PITCH_BEND = 4,
        PROGRAM_CHANGE = 5,
        CHANNEL_PRESSURE = 6,
        KEY_PRESSURE = 7
    };
    
    Type type;              ///< Event type
    uint8_t channel;        ///< MIDI channel (0-15)
    uint8_t data1;          ///< First data byte (note, controller, etc.)
    uint8_t data2;          ///< Second data byte (velocity, value, etc.)
    uint64_t timestamp;     ///< Event timestamp in microseconds
};

/**
 * @brief Plugin information structure
 */
struct PluginInfo {
    const char* name;           ///< Plugin name
    const char* version;        ///< Plugin version
    const char* author;         ///< Plugin author
    const char* description;    ///< Plugin description
};

/**
 * @brief Plugin return codes
 */
enum PluginResult {
    PLUGIN_OK = 0,
    PLUGIN_ERROR = -1,
    PLUGIN_NOT_IMPLEMENTED = -2,
    PLUGIN_INVALID_PARAM = -3
};

/**
 * @brief Abstract interface for NaadCore plugins
 * 
 * All plugins must implement this interface and export
 * the required C-linkage factory functions.
 */
class INaadPlugin {
public:
    virtual ~INaadPlugin() = default;
    
    /**
     * @brief Get plugin information
     * @return Pointer to plugin info structure
     */
    virtual const PluginInfo* get_info() = 0;
    
    /**
     * @brief Initialize the plugin
     * @param audio_driver Audio driver to use (e.g., "alsa", "pipewire")
     * @return PLUGIN_OK on success, error code otherwise
     */
    virtual PluginResult init(const char* audio_driver = nullptr) = 0;
    
    /**
     * @brief Start audio processing
     * @return PLUGIN_OK on success, error code otherwise
     */
    virtual PluginResult start_audio() = 0;
    
    /**
     * @brief Stop audio processing
     * @return PLUGIN_OK on success, error code otherwise
     */
    virtual PluginResult stop_audio() = 0;
    
    /**
     * @brief Process a MIDI event
     * @param event The MIDI event to process
     * @return PLUGIN_OK on success, error code otherwise
     */
    virtual PluginResult handle_midi_event(const MidiEvent& event) = 0;
    
    /**
     * @brief Get plugin-specific configuration
     * @param key Configuration key name
     * @return Configuration value, or empty string if not found
     */
    virtual std::string get_config(const char* key) = 0;
    
    /**
     * @brief Set plugin-specific configuration
     * @param key Configuration key name
     * @param value Configuration value
     * @return PLUGIN_OK on success, error code otherwise
     */
    virtual PluginResult set_config(const char* key, const char* value) = 0;
};

} // namespace naadcore

// ============================================================================
// C-linkage factory functions (exported by plugins)
// ============================================================================

/**
 * @brief Create plugin instance (must be exported by plugin)
 * @return Pointer to new plugin instance, or nullptr on failure
 */
extern "C" {
    typedef naadcore::INaadPlugin* (*PluginCreateFunc)();
    
    /**
     * @brief Destroy plugin instance (must be exported by plugin)
     * @param plugin Plugin instance to destroy
     */
    typedef void (*PluginDestroyFunc)(naadcore::INaadPlugin* plugin);
    
    /**
     * @brief Get plugin API version (must be exported by plugin)
     * @return API version (currently 1)
     */
    typedef int (*PluginGetVersionFunc)();
}

#endif // NAADCORE_PLUGIN_HPP
