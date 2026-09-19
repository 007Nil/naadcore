#include "naadcore/plugin_manager.hpp"
#include "naadcore/midi.hpp"
#include <iostream>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sstream>
#include <algorithm>

namespace naadcore {

/**
 * @brief CLI application that loads plugins and processes MIDI
 */
class NaadCoreCLI {
public:
    NaadCoreCLI();
    ~NaadCoreCLI();
    
    bool parse_args(int argc, char* argv[]);
    int run();
    
    static void print_usage(const char* program_name);
    
private:
    std::string plugin_path_;
    std::string midi_input_str_;
    std::string audio_driver_;
    std::string audio_device_;
    bool show_help_;
    
     void handle_stdin_command(PluginManager& pm, const std::string& command);
};

NaadCoreCLI::NaadCoreCLI() 
    : plugin_path_(""), midi_input_str_(""), audio_driver_("alsa"),
      audio_device_(""), show_help_(false) {
}

NaadCoreCLI::~NaadCoreCLI() {
}

void NaadCoreCLI::print_usage(const char* program_name) {
    std::cout << "NaadCore CLI - Plugin-based synthesizer loader\n"
              << "\n"
              << "Usage: " << program_name << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  --plugin <path>       Path to plugin shared library (.so)\n"
              << "                        (required for plugin mode)\n"
              << "  --midi <client:port>  ALSA sequencer client:port for MIDI input\n"
              << "  --audio-driver <name> Audio driver: alsa, pipewire, pulseaudio\n"
              << "  --audio-device <name> Audio output device passed to the plugin\n"
              << "                        (driver-specific: ALSA PCM name like\n"
              << "                        plughw:CARD=PCH,DEV=0 or \"default\",\n"
              << "                        PulseAudio sink name; unset = plugin\n"
              << "                        default). Applies at start — restart\n"
              << "                        the CLI to change it\n"
              << "  --help                Show this help message\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " --plugin ./plugins/harmonium/harmonium_plugin.so --midi 20:0\n"
              << "  " << program_name << " --plugin ./plugins/synth.so --midi 20:0 --audio-driver pipewire\n"
              << "\n"
              << "Note: SoundFonts are embedded in plugins, not loaded from CLI.\n";
}

bool NaadCoreCLI::parse_args(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"plugin", required_argument, 0, 'p'},
        {"midi", required_argument, 0, 'm'},
        {"audio-driver", required_argument, 0, 'd'},
        {"audio-device", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int options_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:m:d:o:h", long_options, &options_index)) != -1) {
        switch (c) {
            case 'p':
                plugin_path_ = optarg;
                break;
            case 'm':
                midi_input_str_ = optarg;
                break;
            case 'd':
                audio_driver_ = optarg;
                break;
            case 'o':
                audio_device_ = optarg;
                break;
            case 'h':
                show_help_ = true;
                break;
            case '?':
                return false;
            default:
                std::cerr << "Unknown option: " << (char)c << "\n";
                return false;
        }
    }
    
    return true;
}

int NaadCoreCLI::run() {
    // Show help if requested
    if (show_help_) {
        print_usage("naadcore-cli");
        return 0;
    }
    
    // Validate arguments
    if (plugin_path_.empty()) {
        std::cerr << "Error: --plugin is required\n";
        print_usage("naadcore-cli");
        return 1;
    }
    
    // Load plugin
    std::cout << "Loading plugin: " << plugin_path_ << std::endl;
    
    PluginManager& pm = PluginManager::instance();
    pm.set_audio_driver(audio_driver_.c_str());
    // Must be set BEFORE load_plugin(): PluginManager applies the device
    // via set_config("audio_device") before plugin->init() so the plugin
    // puts it into its synth settings before creating its audio driver.
    pm.set_audio_device(audio_device_.c_str());
    PluginResult result = pm.load_plugin(plugin_path_);
    if (result != PLUGIN_OK) {
        std::cerr << "Failed to load plugin: " << plugin_path_ << std::endl;
        return 1;
    }
    
    // Get plugin info
    const PluginInfo* info = pm.get_plugin_info(plugin_path_);
    if (info) {
        std::cout << "Plugin: " << info->name << " v" << info->version << std::endl;
    }
    
    // Start audio
    std::cout << "Starting audio..." << std::endl;
    result = pm.start_all_audio();
    if (result != PLUGIN_OK) {
        std::cerr << "Failed to start audio" << std::endl;
        pm.cleanup();
        return 1;
    }
    
    // Initialize MIDI input
    MidiInput midi_input;
    if (!midi_input_str_.empty()) {
        if (!midi_input.open(midi_input_str_)) {
            std::cerr << "Failed to open MIDI input: " << midi_input_str_ << std::endl;
            pm.stop_all_audio();
            pm.cleanup();
            return 1;
        }
        
        // Note: MidiInput::open() already prints the informative
        // "MIDI input connected: <src> -> <dst>" line.
        
        // Set up MIDI callback to route events to plugin
        midi_input.set_callback([this, &pm](const snd_seq_event_t& ev) {
            MidiEvent event{};
            
            // Convert ALSA event to plugin event
            switch (ev.type) {
                case SND_SEQ_EVENT_NOTEON:
                    event.type = MidiEvent::NOTE_ON;
                    event.channel = ev.data.note.channel;
                    event.data1 = ev.data.note.note;
                    event.data2 = ev.data.note.velocity;
                    break;
                    
                case SND_SEQ_EVENT_NOTEOFF:
                    event.type = MidiEvent::NOTE_OFF;
                    event.channel = ev.data.note.channel;
                    event.data1 = ev.data.note.note;
                    event.data2 = 0;
                    break;
                    
                case SND_SEQ_EVENT_KEYPRESS:
                    event.type = MidiEvent::KEY_PRESSURE;
                    event.channel = ev.data.note.channel;
                    event.data1 = ev.data.note.note;
                    event.data2 = ev.data.note.velocity;
                    break;
                    
                case SND_SEQ_EVENT_CONTROLLER:
                    event.type = MidiEvent::CONTROL_CHANGE;
                    event.channel = ev.data.control.channel;
                    event.data1 = ev.data.control.param;
                    event.data2 = ev.data.control.value;
                    break;
                    
                case SND_SEQ_EVENT_PITCHBEND: {
                    event.type = MidiEvent::PITCH_BEND;
                    event.channel = ev.data.control.channel;
                    // ALSA pitch bend is -8192 to 8191, convert to 0-16383
                    int bend = ev.data.control.value + 8192;
                    event.data1 = bend & 0x7F;
                    event.data2 = (bend >> 7) & 0x7F;
                    break;
                }
                    
                case SND_SEQ_EVENT_PGMCHANGE:
                    event.type = MidiEvent::PROGRAM_CHANGE;
                    event.channel = ev.data.control.channel;
                    event.data1 = ev.data.control.value;
                    event.data2 = 0;
                    break;
                    
                case SND_SEQ_EVENT_CHANPRESS:
                    event.type = MidiEvent::CHANNEL_PRESSURE;
                    event.channel = ev.data.control.channel;
                    event.data1 = ev.data.control.value;
                    event.data2 = 0;
                    break;
                    
                default:
                    // Ignore unsupported events
                    return;
            }
            
            // Route event to plugin
            pm.route_midi_event(event);
        });
    }
    
    // Setup signal handlers - use a static variable to track running state
    static volatile sig_atomic_t running_flag = 1;
    struct sigaction sa;
    sa.sa_handler = [](int signum) {
        std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
        running_flag = 0;
    };
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    // Ready message
    std::cout << "Ready. Press Ctrl+C to exit." << std::endl;
    if (!midi_input_str_.empty()) {
        std::cout << "Listening for MIDI from " << midi_input_str_ << std::endl;
    }
    std::cout << "CLI commands: coupler on|off, status" << std::endl;
    
    // Setup stdin for non-blocking reads using select()
    int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
    
    // Main loop - process MIDI events and stdin commands
    while (running_flag) {
        // Create fd_set for select()
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        int max_fd = STDIN_FILENO;
        if (!midi_input_str_.empty()) {
            // Get the fd from MidiInput
            int midi_fd = midi_input.get_fd();
            if (midi_fd >= 0) {
                FD_SET(midi_fd, &read_fds);
                max_fd = std::max(max_fd, midi_fd);
            }
        }
        
        // Use a small timeout to allow checking running_flag
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms
        
        int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            std::cerr << "Select error: " << strerror(errno) << std::endl;
            break;
        }
        
        // Check for stdin activity
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char buffer[1024];
            ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                std::string input(buffer);
                
                // Process complete lines
                size_t pos = 0;
                size_t newline_pos;
                
                while ((newline_pos = input.find('\n', pos)) != std::string::npos) {
                    std::string command = input.substr(pos, newline_pos - pos);
                    // Remove leading/trailing whitespace
                    command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }));
                    command.erase(std::find_if(command.rbegin(), command.rend(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }).base(), command.end());
                    
                    if (!command.empty()) {
                        handle_stdin_command(pm, command);
                    }
                    pos = newline_pos + 1;
                }
            }
        }
        
        // Process MIDI events if configured
        if (!midi_input_str_.empty()) {
            int events = midi_input.process_events();
            (void)events; // Suppress unused variable warning
        }
        
        // Small sleep to prevent busy waiting
        usleep(1000);
    }
    
    // Restore stdin flags
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
    
    // Cleanup
    std::cout << "Cleaning up..." << std::endl;
    pm.stop_all_audio();
    pm.cleanup();
    
    return 0;
}

// ============================================================================
// CLI command handler
// ============================================================================

void NaadCoreCLI::handle_stdin_command(PluginManager& pm, const std::string& command) {
    // Split command into parts
    std::istringstream iss(command);
    std::vector<std::string> tokens;
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    if (tokens.empty()) {
        return;
    }
    
    // Handle commands
    if (tokens[0] == "coupler") {
        if (tokens.size() < 2) {
            std::cout << "Usage: coupler on|off" << std::endl;
            return;
        }
        
        if (tokens[1] == "on") {
            PluginResult result = pm.set_plugin_config(plugin_path_, "coupler", "on");
            if (result == PLUGIN_OK) {
                std::cout << "Coupler ON" << std::endl;
            } else {
                std::cout << "Failed to set coupler ON" << std::endl;
            }
        } else if (tokens[1] == "off") {
            PluginResult result = pm.set_plugin_config(plugin_path_, "coupler", "off");
            if (result == PLUGIN_OK) {
                std::cout << "Coupler OFF" << std::endl;
            } else {
                std::cout << "Failed to set coupler OFF" << std::endl;
            }
        } else {
            std::cout << "Invalid coupler command. Usage: coupler on|off" << std::endl;
        }
    } else if (tokens[0] == "status") {
        std::string value = pm.get_plugin_config(plugin_path_, "coupler");
        if (!value.empty()) {
            std::cout << "Coupler: " << value << std::endl;
        } else {
            std::cout << "No coupler configuration found" << std::endl;
        }
    } else {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << "Available commands: coupler on, coupler off, status" << std::endl;
    }
}

} // namespace naadcore

// ============================================================================
// Main entry point
// ============================================================================

int main(int argc, char* argv[]) {
    naadcore::NaadCoreCLI cli;
    
    if (!cli.parse_args(argc, argv)) {
        return 1;
    }
    
    return cli.run();
}
