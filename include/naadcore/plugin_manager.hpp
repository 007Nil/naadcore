#ifndef NAADCORE_PLUGIN_MANAGER_HPP
#define NAADCORE_PLUGIN_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

#include "naadcore/plugin.hpp"

namespace naadcore {

/**
 * @brief Manages plugin loading, unloading, and event routing
 * 
 * PluginManager is a singleton that handles all plugin operations:
 * - Loading plugins from shared libraries
 * - Unloading plugins
 * - Routing MIDI events to loaded plugins
 * - Managing plugin lifecycle
 */
class PluginManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to PluginManager instance
     */
    static PluginManager& instance();
    
    /**
     * @brief Load a plugin from the given path
     * @param path Path to the plugin shared library (.so file)
     * @return PLUGIN_OK on success, error code otherwise
     */
    PluginResult load_plugin(const std::string& path);
    
    /**
     * @brief Unload a plugin from the given path
     * @param path Path to the plugin shared library
     * @return PLUGIN_OK on success, error code otherwise
     */
    PluginResult unload_plugin(const std::string& path);
    
    /**
     * @brief Route a MIDI event to the loaded plugin
     * @param event MIDI event to route
     * @return PLUGIN_OK on success, error code otherwise
     */
    PluginResult route_midi_event(const MidiEvent& event);
    
    /**
     * @brief Get information about a loaded plugin
     * @param path Path to the plugin
     * @return Pointer to PluginInfo, or nullptr if not loaded
     */
    const PluginInfo* get_plugin_info(const std::string& path) const;
    
    /**
     * @brief Check if a plugin is loaded
     * @param path Path to the plugin
     * @return true if plugin is loaded, false otherwise
     */
    bool is_plugin_loaded(const std::string& path) const;
    
    /**
     * @brief Get list of loaded plugin paths
     * @return Vector of loaded plugin paths
     */
    std::vector<std::string> get_loaded_plugins() const;
    
    /**
     * @brief Set the audio driver passed to plugins at init
     * @param audio_driver Audio driver name (e.g. "alsa", "pipewire",
     *        "pulseaudio"), or nullptr for the plugin's own default
     */
    void set_audio_driver(const char* audio_driver);

    /**
     * @brief Set the audio output device handed to plugins via the config
     *        seam before init (well-known "audio_device" key)
     * @param audio_device Device name (machine-specific: e.g. an ALSA PCM
     *        name or a PulseAudio sink name), or nullptr/empty for the
     *        plugin's own default
     */
    void set_audio_device(const char* audio_device);

    /**
     * @brief Initialize plugin system
     * @param audio_driver Audio driver to use for plugins
     * @return PLUGIN_OK on success, error code otherwise
     */
    PluginResult initialize(const char* audio_driver = nullptr);
    
    /**
     * @brief Start audio for all plugins
     * @return PLUGIN_OK on success, error code otherwise
     */
    PluginResult start_all_audio();
    
    /**
     * @brief Stop audio for all plugins
     * @return PLUGIN_OK on success, error code otherwise
     */
    PluginResult stop_all_audio();
    
    /**
     * @brief Clean up and destroy all plugins
     */
    void cleanup();

private:
    PluginManager();
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    
    struct PluginHandle {
        void* handle;                     ///< DLOPEN handle
        INaadPlugin* plugin;             ///< Plugin instance
        PluginCreateFunc create_func;    ///< Plugin creation function
        PluginDestroyFunc destroy_func;  ///< Plugin destruction function
    };
    
    std::unordered_map<std::string, PluginHandle> plugins_;
    mutable std::recursive_mutex mutex_;
    std::string audio_driver_;
    std::string audio_device_;
};

} // namespace naadcore

#endif // NAADCORE_PLUGIN_MANAGER_HPP
