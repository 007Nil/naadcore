#include <dlfcn.h>
#include <iostream>
#include <algorithm>
#include <vector>

#include "naadcore/plugin_manager.hpp"

namespace naadcore {

namespace {

/**
 * Apply the well-known "audio_device" environment key to a plugin via the
 * config seam. MUST be called before plugin->init() so the device reaches
 * the plugin's synth settings before it creates its audio driver.
 *
 * The result is advisory: PLUGIN_OK is expected, PLUGIN_NOT_IMPLEMENTED is
 * acceptable degradation (a plugin that does not know the key just runs on
 * its own default), and any other result is logged as a warning but never
 * fails the load/init — a genuinely unusable device fails later, at
 * start_audio(), where the error is actionable.
 */
void apply_audio_device_config(INaadPlugin* plugin, const std::string& id,
                               const std::string& device) {
    if (device.empty()) {
        return;
    }
    const PluginResult cfg = plugin->set_config("audio_device", device.c_str());
    if (cfg != PLUGIN_OK && cfg != PLUGIN_NOT_IMPLEMENTED) {
        std::cerr << "Warning: plugin did not accept the audio_device key ("
                  << id << ", result " << cfg << ")" << std::endl;
    }
}

} // namespace

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() {
    cleanup();
}

PluginManager& PluginManager::instance() {
    static PluginManager instance;
    return instance;
}

PluginResult PluginManager::load_plugin(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Check if already loaded
    if (is_plugin_loaded(path)) {
        return PLUGIN_OK;
    }
    
    // Load the shared library
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "Failed to load plugin '" << path << "': " << dlerror() << std::endl;
        return PLUGIN_ERROR;
    }
    dlerror(); // Clear any existing error
    
    // Load required functions
    PluginCreateFunc create_func = (PluginCreateFunc)dlsym(handle, "naad_plugin_create");
    const char* error = dlerror();
    if (error) {
        std::cerr << "Failed to load naad_plugin_create: " << error << std::endl;
        dlclose(handle);
        return PLUGIN_ERROR;
    }

    PluginDestroyFunc destroy_func = (PluginDestroyFunc)dlsym(handle, "naad_plugin_destroy");
    error = dlerror();
    if (error) {
        std::cerr << "Failed to load naad_plugin_destroy: " << error << std::endl;
        dlclose(handle);
        return PLUGIN_ERROR;
    }
    
    // Create plugin instance
    INaadPlugin* plugin = create_func();
    if (!plugin) {
        std::cerr << "Plugin creation failed for: " << path << std::endl;
        dlclose(handle);
        return PLUGIN_ERROR;
    }

    // Ordering constraint (load-bearing): the audio output device must
    // reach the plugin via set_config BEFORE init(), so the plugin can put
    // it into its synth settings before it creates its audio driver at
    // start_audio(). (The driver has an init() parameter channel; the
    // device deliberately does not — no interface signature change.)
    apply_audio_device_config(plugin, path, audio_device_);

    // Initialize plugin
    PluginResult result = plugin->init(audio_driver_.empty() ? nullptr : audio_driver_.c_str());
    if (result != PLUGIN_OK) {
        std::cerr << "Plugin initialization failed for: " << path << std::endl;
        destroy_func(plugin);
        dlclose(handle);
        return result;
    }
    
    // Store plugin handle
    PluginHandle plugin_handle;
    plugin_handle.handle = handle;
    plugin_handle.plugin = plugin;
    plugin_handle.create_func = create_func;
    plugin_handle.destroy_func = destroy_func;
    
    plugins_[path] = plugin_handle;

    const PluginInfo* info = plugin->get_info();
    if (info) {
        std::cout << "Loaded plugin: " << info->name 
                  << " v" << info->version << " (" << path << ")" << std::endl;
    } else {
        std::cout << "Loaded plugin: " << path << std::endl;
    }
    
    return PLUGIN_OK;
}

PluginResult PluginManager::unload_plugin(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = plugins_.find(path);
    if (it == plugins_.end()) {
        std::cerr << "Plugin not loaded: " << path << std::endl;
        return PLUGIN_ERROR;
    }
    
    PluginHandle& handle = it->second;
    
    // Stop audio if running
    handle.plugin->stop_audio();
    
    // Destroy plugin instance
    if (handle.destroy_func && handle.plugin) {
        handle.destroy_func(handle.plugin);
        handle.plugin = nullptr;
    }
    
    // Close the shared library
    if (handle.handle) {
        dlclose(handle.handle);
        handle.handle = nullptr;
    }
    
    plugins_.erase(it);
    
    std::cout << "Unloaded plugin: " << path << std::endl;
    return PLUGIN_OK;
}

PluginResult PluginManager::route_midi_event(const MidiEvent& event) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (plugins_.empty()) {
        std::cerr << "No plugins loaded" << std::endl;
        return PLUGIN_ERROR;
    }
    
    // Route event to all loaded plugins
    PluginResult result = PLUGIN_OK;
    for (auto& pair : plugins_) {
        PluginResult r = pair.second.plugin->handle_midi_event(event);
        if (r != PLUGIN_OK) {
            result = r;
        }
    }
    
    return result;
}

const PluginInfo* PluginManager::get_plugin_info(const std::string& path) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = plugins_.find(path);
    if (it == plugins_.end()) {
        return nullptr;
    }
    
    return it->second.plugin->get_info();
}

bool PluginManager::is_plugin_loaded(const std::string& path) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return plugins_.find(path) != plugins_.end();
}

std::vector<std::string> PluginManager::get_loaded_plugins() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::vector<std::string> paths;
    paths.reserve(plugins_.size());
    
    for (const auto& pair : plugins_) {
        paths.push_back(pair.first);
    }
    
    return paths;
}

void PluginManager::set_audio_driver(const char* audio_driver) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    audio_driver_ = audio_driver ? audio_driver : "";
}

void PluginManager::set_audio_device(const char* audio_device) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    audio_device_ = audio_device ? audio_device : "";
}

PluginResult PluginManager::initialize(const char* audio_driver) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    audio_driver_ = audio_driver ? audio_driver : "alsa";
    
    // Initialize all loaded plugins. The audio_device key is re-applied
    // via the config seam before each init() (same ordering constraint as
    // load_plugin: the device must be in the plugin's settings before its
    // audio driver is created).
    for (auto& pair : plugins_) {
        apply_audio_device_config(pair.second.plugin, pair.first,
                                  audio_device_);
        PluginResult result = pair.second.plugin->init(audio_driver_.c_str());
        if (result != PLUGIN_OK) {
            std::cerr << "Failed to reinitialize plugin: " << pair.first << std::endl;
            return result;
        }
    }
    
    return PLUGIN_OK;
}

PluginResult PluginManager::start_all_audio() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    PluginResult result = PLUGIN_OK;
    for (auto& pair : plugins_) {
        PluginResult r = pair.second.plugin->start_audio();
        if (r != PLUGIN_OK) {
            result = r;
        }
    }
    
    return result;
}

PluginResult PluginManager::stop_all_audio() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    PluginResult result = PLUGIN_OK;
    for (auto& pair : plugins_) {
        PluginResult r = pair.second.plugin->stop_audio();
        if (r != PLUGIN_OK) {
            result = r;
        }
    }
    
    return result;
}

void PluginManager::cleanup() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    for (auto& pair : plugins_) {
        PluginHandle& handle = pair.second;
        
        if (handle.plugin) {
            handle.plugin->stop_audio();
            if (handle.destroy_func) {
                handle.destroy_func(handle.plugin);
            }
            handle.plugin = nullptr;
        }
        
        if (handle.handle) {
            dlclose(handle.handle);
            handle.handle = nullptr;
        }
    }
    
    plugins_.clear();
}

} // namespace naadcore
