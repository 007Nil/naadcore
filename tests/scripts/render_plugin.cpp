// Ad-hoc offline renderer with the harmonium plugin IN the loop.
//
// Usage: render_plugin <libharmonium_plugin.so> <track.mid> <out.wav>
//                     [tail_seconds (default 3)]
//
// Why this exists: the fluidsynth CLI used by tests/scripts/render_sf2.sh
// renders with stock synth settings and cannot load a NaadCore plugin, so
// plugin-only behavior (Phase 1 voicing, Phase 2 envelope generators,
// uniform bellows velocity, future stops/couplers) never shows up in
// offline renders — until now. This harness loads the actual plugin .so
// via dlopen (the same mechanism as PluginManager/the CLI), initializes it
// with the FluidSynth "file" audio driver, feeds the MIDI file's events at
// their file times over the wall clock, and moves the rendered WAV to the
// requested output.
//
// Timing model: FluidSynth's "file" audio driver renders in real time (its
// timer is wall-clock throttled with absolute positioning), so the harness
// simply sleeps until each event's time and hands it to the plugin, like a
// keyboard would. Events land in the WAV within a few ms of their nominal
// times (timer thread start jitter) — fine for the 5.8 ms analysis
// resolution of tests/analyze.py. The driver writes ./fluidsynth.wav
// (FluidSynth's default audio.file.name), so the harness chdirs into the
// output directory for the duration of the render.
//
// Build & run via tests/scripts/run_render_plugin.sh (ad hoc, not in the
// CMake build). No dlclose() at the end — same teardown-crash caveat as
// tests/test_plugin_config.cpp.

#include "naadcore/plugin.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

struct PendingEvent {
    uint32_t tick;
    uint32_t seq;                 // file order (stable within a tick)
    naadcore::MidiEvent event;
};

struct TempoChange {
    uint32_t tick;
    uint32_t uspq;                // microseconds per quarter note
};

struct Timed {
    double seconds;
    naadcore::MidiEvent event;
};

uint32_t read_be16(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 8) | p[1];
}

uint32_t read_be24(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 16) |
           (static_cast<uint32_t>(p[1]) << 8) | p[2];
}

// Variable-length quantity (1-4 bytes, 7 bits each, MSB = continuation).
bool read_vlq(const uint8_t*& p, const uint8_t* end, uint32_t& out) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        if (p >= end) {
            return false;
        }
        const uint8_t b = *p++;
        v = (v << 7) | (b & 0x7Fu);
        if ((b & 0x80u) == 0) {
            out = v;
            return true;
        }
    }
    return false;
}

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "error: cannot open " << path << std::endl;
        return false;
    }
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    f.read(reinterpret_cast<char*>(out.data()), n);
    return f.good() || f.eof();
}

// Parses one MTrk chunk, appending channel events (tempo-mapped later).
// Handles running status; skips meta/sysex but captures Set Tempo.
bool parse_track(const uint8_t* p, const uint8_t* end, uint32_t division,
                 std::vector<PendingEvent>& events,
                 std::vector<TempoChange>& tempos, uint32_t& seq) {
    uint32_t tick = 0;
    uint8_t running = 0;
    while (p < end) {
        uint32_t delta = 0;
        if (!read_vlq(p, end, delta)) {
            std::cerr << "error: bad delta time" << std::endl;
            return false;
        }
        tick += delta;
        if (p >= end) {
            std::cerr << "error: truncated track" << std::endl;
            return false;
        }
        uint8_t status = *p;
        if (status < 0x80u) {
            // running status: reuse last channel-message status
            status = running;
        } else {
            ++p;
            if (status < 0xF0u) {
                running = status;
            }
        }
        if (status == 0xFFu) {
            // meta event: type, VLQ length, data
            if (p >= end) {
                return false;
            }
            const uint8_t type = *p++;
            uint32_t len = 0;
            if (!read_vlq(p, end, len) ||
                static_cast<size_t>(end - p) < len) {
                return false;
            }
            if (type == 0x51u && len == 3) {  // Set Tempo
                tempos.push_back({tick, read_be24(p)});
            }
            if (type == 0x2Fu) {              // End of Track
                break;
            }
            p += len;
        } else if (status == 0xF0u || status == 0xF7u) {
            // sysex: VLQ length + data
            uint32_t len = 0;
            if (!read_vlq(p, end, len) ||
                static_cast<size_t>(end - p) < len) {
                return false;
            }
            p += len;
        } else if (status >= 0x80u && status < 0xF0u) {
            const uint8_t kind = status & 0xF0u;
            const int nbytes = (kind == 0xC0u || kind == 0xD0u) ? 1 : 2;
            if (static_cast<size_t>(end - p) < static_cast<size_t>(nbytes)) {
                return false;
            }
            naadcore::MidiEvent ev{};
            ev.timestamp = 0;
            ev.channel = status & 0x0Fu;
            switch (kind) {
                case 0x90u:
                    ev.type = (p[1] > 0) ? naadcore::MidiEvent::NOTE_ON
                                         : naadcore::MidiEvent::NOTE_OFF;
                    ev.data1 = p[0];
                    ev.data2 = p[1];
                    break;
                case 0x80u:
                    ev.type = naadcore::MidiEvent::NOTE_OFF;
                    ev.data1 = p[0];
                    ev.data2 = p[1];
                    break;
                case 0xA0u:
                    ev.type = naadcore::MidiEvent::KEY_PRESSURE;
                    ev.data1 = p[0];
                    ev.data2 = p[1];
                    break;
                case 0xB0u:
                    ev.type = naadcore::MidiEvent::CONTROL_CHANGE;
                    ev.data1 = p[0];
                    ev.data2 = p[1];
                    break;
                case 0xC0u:
                    ev.type = naadcore::MidiEvent::PROGRAM_CHANGE;
                    ev.data1 = p[0];
                    ev.data2 = 0;
                    break;
                case 0xD0u:
                    ev.type = naadcore::MidiEvent::CHANNEL_PRESSURE;
                    ev.data1 = p[0];
                    ev.data2 = 0;
                    break;
                case 0xE0u:
                    ev.type = naadcore::MidiEvent::PITCH_BEND;
                    ev.data1 = p[0];  // LSB
                    ev.data2 = p[1];  // MSB (plugin: value = lsb | msb << 7)
                    break;
                default:
                    p += nbytes;
                    continue;
            }
            events.push_back({tick, seq++, ev});
            p += nbytes;
        } else {
            std::cerr << "error: unsupported status byte "
                      << static_cast<int>(status) << std::endl;
            return false;
        }
    }
    (void)division;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: " << argv[0]
                  << " <libharmonium_plugin.so> <track.mid> <out.wav>"
                     " [tail_seconds]" << std::endl;
        return 1;
    }
    const std::string plugin_path = argv[1];
    const std::string track_path = argv[2];
    const std::string out_path = argv[3];
    const double tail_s = (argc > 4) ? std::stod(argv[4]) : 3.0;

    // ---- load MIDI file -------------------------------------------------
    std::vector<uint8_t> midi;
    if (!read_file(track_path, midi) || midi.size() < 14 ||
        std::memcmp(midi.data(), "MThd", 4) != 0) {
        std::cerr << "error: not a MIDI file: " << track_path << std::endl;
        return 1;
    }
    const uint32_t header_len = (static_cast<uint32_t>(midi[4]) << 24) |
                                 (static_cast<uint32_t>(midi[5]) << 16) |
                                 (static_cast<uint32_t>(midi[6]) << 8) |
                                 static_cast<uint32_t>(midi[7]);
    if (header_len < 6) {
        std::cerr << "error: short MThd" << std::endl;
        return 1;
    }
    const uint32_t ntracks = read_be16(midi.data() + 10);
    const uint32_t division = read_be16(midi.data() + 12);
    if ((division & 0x8000u) != 0) {
        std::cerr << "error: SMPTE timing not supported" << std::endl;
        return 1;
    }

    std::vector<PendingEvent> events;
    std::vector<TempoChange> tempos;
    uint32_t seq = 0;
    size_t pos = 8 + header_len;
    for (uint32_t t = 0; t < ntracks; ++t) {
        if (pos + 8 > midi.size() ||
            std::memcmp(midi.data() + pos, "MTrk", 4) != 0) {
            std::cerr << "error: bad MTrk chunk" << std::endl;
            return 1;
        }
        const uint32_t len = (static_cast<uint32_t>(midi[pos + 4]) << 24) |
                              (static_cast<uint32_t>(midi[pos + 5]) << 16) |
                              (static_cast<uint32_t>(midi[pos + 6]) << 8) |
                              midi[pos + 7];
        pos += 8;
        if (pos + len > midi.size()) {
            std::cerr << "error: truncated track chunk" << std::endl;
            return 1;
        }
        if (!parse_track(midi.data() + pos, midi.data() + pos + len,
                         division, events, tempos, seq)) {
            return 1;
        }
        pos += len;
    }

    // ---- tempo-map ticks to seconds --------------------------------------
    std::stable_sort(events.begin(), events.end(),
                     [](const PendingEvent& a, const PendingEvent& b) {
                         return a.tick < b.tick;
                     });
    std::stable_sort(tempos.begin(), tempos.end(),
                     [](const TempoChange& a, const TempoChange& b) {
                         return a.tick < b.tick;
                     });
    std::vector<Timed> timed;
    timed.reserve(events.size());
    uint32_t uspq = 500000;  // 120 BPM default
    double sec = 0.0;
    uint32_t last_tick = 0;
    size_t next_tempo = 0;
    const double tpqn = static_cast<double>(division ? division : 480);
    for (const PendingEvent& e : events) {
        while (next_tempo < tempos.size() &&
               tempos[next_tempo].tick <= e.tick) {
            sec += (tempos[next_tempo].tick - last_tick) * uspq
                   / (1e6 * tpqn);
            last_tick = tempos[next_tempo].tick;
            uspq = tempos[next_tempo].uspq;
            ++next_tempo;
        }
        sec += (e.tick - last_tick) * uspq / (1e6 * tpqn);
        last_tick = e.tick;
        timed.push_back({sec, e.event});
    }
    // length = time of the last tempo change or event, plus track remainder:
    // use the last event time as the track length for tail computation.
    const double track_len = timed.empty() ? 0.0 : timed.back().seconds;

    // ---- load plugin -----------------------------------------------------
    void* handle = dlopen(plugin_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return 1;
    }
    auto create = reinterpret_cast<PluginCreateFunc>(
        dlsym(handle, "naad_plugin_create"));
    auto destroy = reinterpret_cast<PluginDestroyFunc>(
        dlsym(handle, "naad_plugin_destroy"));
    if (!create || !destroy) {
        std::cerr << "missing plugin exports" << std::endl;
        return 1;
    }
    auto* plugin = create();
    if (plugin->init("file") != naadcore::PLUGIN_OK) {
        std::cerr << "plugin init failed" << std::endl;
        return 1;
    }

    // ---- render into the output directory --------------------------------
    std::string out_dir = ".";
    std::string out_base = out_path;
    const size_t slash = out_path.find_last_of('/');
    if (slash != std::string::npos) {
        out_dir = out_path.substr(0, slash);
        out_base = out_path.substr(slash + 1);
    }
    char orig_cwd[4096];
    if (getcwd(orig_cwd, sizeof(orig_cwd)) == nullptr) {
        std::cerr << "getcwd failed" << std::endl;
        return 1;
    }
    if (chdir(out_dir.c_str()) != 0) {
        std::cerr << "error: cannot chdir to " << out_dir << std::endl;
        return 1;
    }

    if (plugin->start_audio() != naadcore::PLUGIN_OK) {
        std::cerr << "start_audio failed (is the 'file' audio driver "
                     "compiled into FluidSynth?)" << std::endl;
        return 1;
    }

    const double lead_in = 0.2;
    const auto t0 = std::chrono::steady_clock::now();
    const auto wav_time = [&](double s) {
        return t0 + std::chrono::duration_cast<
                        std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(lead_in + s));
    };
    for (const Timed& t : timed) {
        std::this_thread::sleep_until(wav_time(t.seconds));
        if (plugin->handle_midi_event(t.event) != naadcore::PLUGIN_OK) {
            std::cerr << "warning: handle_midi_event failed" << std::endl;
        }
    }
    std::this_thread::sleep_until(
        wav_time(track_len + (tail_s > 0.5 ? tail_s : 0.5)));
    plugin->stop_audio();

    if (std::rename("fluidsynth.wav", out_base.c_str()) != 0) {
        std::cerr << "error: no fluidsynth.wav produced — rename failed"
                  << std::endl;
        if (chdir(orig_cwd) != 0) { /* best effort */ }
        return 1;
    }
    if (chdir(orig_cwd) != 0) { /* best effort */ }

    std::cout << "rendered " << timed.size() << " events ("
              << track_len + lead_in + tail_s << " s audio) -> "
              << out_path << std::endl;

    // No dlclose(): unmapping FluidSynth/GLib crashes in teardown on this
    // system (see tests/test_plugin_config.cpp). The process exit cleans up.
    return 0;
}
