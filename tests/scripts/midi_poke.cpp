// midi_poke.cpp — ad-hoc ALSA sequencer VIRTUAL MIDI SOURCE for the e2e
// harness (tests/e2e_cli_coupler.sh Check 5).
//
// Usage: midi_poke
//
// Creates an ALSA sequencer client ("naadcore-e2e-source") with one
// READ-capable port ("e2e source") — exactly how a hardware keyboard
// exposes itself — so the naadcore CLI can subscribe to it as its MIDI
// input via `--midi <client>:<port>` (the CLI's MidiInput::open does the
// subscribing). Announces the port on stdout, then executes a simple
// command script from stdin (one command per line), so the harness can
// inject real NoteOn/NoteOff events into the FULL chain:
//   stdin command -> PluginManager -> plugin set_config
//   poke NoteOn -> ALSA seq -> CLI MidiInput -> PluginManager
//     -> plugin handle_midi_event -> FluidSynth render -> acoustic assert
//
// Commands (each acked on stdout as "OK <args>"; EOF or "exit" quits):
//   on <note> <vel> [channel]   send NOTEON   (defaults: channel 0)
//   off <note> [channel]        send NOTEOFF  (defaults: channel 0)
//   wait <ms>                   sleep
//   waitfile <path> [timeout_s] block until <path> exists (default
//                               timeout 30 s; fails the process if the
//                               file never appears) — lets the harness
//                               synchronize the timeline to the CLI being
//                               up ("Listening for MIDI" in its output)
//   exit                        quit cleanly
//
// Built ad hoc by the harness into its scratch dir (never committed as a
// binary): g++ -std=c++17 -Wall -Wextra -Wpedantic midi_poke.cpp -lasound

#include <alsa/asoundlib.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <fstream>

namespace {

// Read an optional trailing integer from a command line: absent value
// yields the default without poisoning the stream state (std::istream
// sets failbit on a missing extraction, which would reject the command).
int opt_int(std::istringstream& iss, int fallback) {
    int v = 0;
    if (iss >> v) {
        return v;
    }
    iss.clear();
    return fallback;
}

bool wait_for_file(const std::string& path, int timeout_s) {
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::seconds(timeout_s);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream f(path);
        if (f.good()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

} // namespace

int main() {
    snd_seq_t* seq = nullptr;
    int err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0);
    if (err < 0) {
        std::cerr << "midi_poke: cannot open ALSA sequencer: "
                  << snd_strerror(err) << std::endl;
        return 1;
    }
    snd_seq_set_client_name(seq, "naadcore-e2e-source");
    // READ + SUBS_READ: other clients (the CLI) may subscribe to this
    // port as their event SOURCE — the same caps a keyboard port has.
    const int port = snd_seq_create_simple_port(
        seq, "e2e source",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        std::cerr << "midi_poke: cannot create port: "
                  << snd_strerror(port) << std::endl;
        snd_seq_close(seq);
        return 1;
    }
    std::cout << "PORT " << snd_seq_client_id(seq) << ":" << port
              << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd.empty() || cmd[0] == '#') {
            continue;
        }
        if (cmd == "exit") {
            std::cout << "OK exit" << std::endl;
            break;
        }
        if (cmd == "on" || cmd == "off") {
            int note = -1;
            int vel = (cmd == "on") ? -1 : 0;
            iss >> note;
            if (cmd == "on") {
                iss >> vel;
            }
            const int channel = opt_int(iss, 0);
            if (iss.fail() || note < 0 || note > 127
                || (cmd == "on" && (vel < 1 || vel > 127))
                || channel < 0 || channel > 15) {
                std::cerr << "midi_poke: bad command: " << line << std::endl;
                snd_seq_close(seq);
                return 1;
            }
            snd_seq_event_t ev;
            snd_seq_ev_clear(&ev);
            snd_seq_ev_set_source(&ev, port);
            snd_seq_ev_set_subs(&ev);    // deliver to every subscriber
            snd_seq_ev_set_direct(&ev);  // immediate, no scheduling queue
            if (cmd == "on") {
                snd_seq_ev_set_noteon(&ev, channel, note, vel);
            } else {
                snd_seq_ev_set_noteoff(&ev, channel, note, 0);
            }
            // output_direct: unbuffered, bypasses the event pool —
            // simplest correct delivery for a few sparse events.
            err = snd_seq_event_output_direct(seq, &ev);
            if (err < 0) {
                std::cerr << "midi_poke: event send failed: "
                          << snd_strerror(err) << std::endl;
                snd_seq_close(seq);
                return 1;
            }
            std::cout << "OK " << line << std::endl;
        } else if (cmd == "wait") {
            int ms = -1;
            iss >> ms;
            if (iss.fail() || ms < 0) {
                std::cerr << "midi_poke: bad command: " << line << std::endl;
                snd_seq_close(seq);
                return 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            std::cout << "OK " << line << std::endl;
        } else if (cmd == "waitfile") {
            std::string path;
            iss >> path;
            const int timeout_s = opt_int(iss, 30);
            if (iss.fail() || path.empty()) {
                std::cerr << "midi_poke: bad command: " << line << std::endl;
                snd_seq_close(seq);
                return 1;
            }
            if (!wait_for_file(path, timeout_s)) {
                std::cerr << "midi_poke: timed out waiting for " << path
                          << std::endl;
                snd_seq_close(seq);
                return 1;
            }
            std::cout << "OK waitfile " << path << std::endl;
        } else {
            std::cerr << "midi_poke: unknown command: " << line << std::endl;
            snd_seq_close(seq);
            return 1;
        }
        // Keep the announcement + acks promptly visible to the harness.
        std::cout.flush();
    }
    snd_seq_close(seq);
    return 0;
}