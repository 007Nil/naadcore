// Config-seam tests for the harmonium plugin (gain/reverb/chorus, Phase 2
// envelope attack_ms/release_ms live keys, Phase 3 stop key).
//
// Loads libharmonium_plugin.so via dlopen (same mechanism as PluginManager)
// and exercises set_config/get_config, including live changes after init().
// init() alone does not open an audio device, so this runs headless.
//
// Build & run via tests/scripts/run_config_tests.sh.

#include "naadcore/plugin.hpp"
#include <dlfcn.h>
#include <iostream>
#include <string>

static int failures = 0;
static int checks = 0;

static void check_eq(const std::string& what, const std::string& got,
                     const std::string& want) {
    ++checks;
    if (got != want) {
        ++failures;
        std::cout << "FAIL " << what << ": got '" << got << "' want '" << want
                  << "'" << std::endl;
    }
}

static void check_result(const std::string& what, naadcore::PluginResult got,
                         naadcore::PluginResult want) {
    ++checks;
    if (got != want) {
        ++failures;
        std::cout << "FAIL " << what << ": got " << got << " want " << want
                  << std::endl;
    }
}

int main(int argc, char** argv) {
    const char* path =
        argc > 1 ? argv[1] : "build/plugins/libharmonium_plugin.so";
    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return 1;
    }
    auto create = reinterpret_cast<PluginCreateFunc>(
        dlsym(handle, "naad_plugin_create"));
    auto destroy = reinterpret_cast<PluginDestroyFunc>(
        dlsym(handle, "naad_plugin_destroy"));
    auto get_version = reinterpret_cast<PluginGetVersionFunc>(
        dlsym(handle, "naad_plugin_get_version"));
    if (!create || !destroy || !get_version) {
        std::cerr << "missing plugin exports" << std::endl;
        return 1;
    }

    check_eq("plugin api version", std::to_string(get_version()), "1");

    auto* p = create();

    // defaults (before init)
    check_eq("default gain", p->get_config("gain"), "0.400");
    check_eq("default reverb", p->get_config("reverb"), "on");
    check_eq("default chorus", p->get_config("chorus"), "off");
    check_eq("default audio_driver", p->get_config("audio_driver"), "alsa");
    check_eq("default attack_ms", p->get_config("attack_ms"), "10");
    check_eq("default release_ms", p->get_config("release_ms"), "200");
    check_eq("default stop", p->get_config("stop"), "single");
    check_eq("default coupler", p->get_config("coupler"), "off");
    check_eq("default sub_octave", p->get_config("sub_octave"), "off");

    // init (no audio driver started)
    check_result("init", p->init(nullptr), naadcore::PLUGIN_OK);
    check_eq("gain after init", p->get_config("gain"), "0.400");
    check_eq("attack_ms after init", p->get_config("attack_ms"), "10");
    check_eq("release_ms after init", p->get_config("release_ms"), "200");

    // live gain
    check_result("gain 1.5", p->set_config("gain", "1.5"),
                 naadcore::PLUGIN_OK);
    check_eq("gain reads 1.500", p->get_config("gain"), "1.500");
    check_result("gain 0", p->set_config("gain", "0"), naadcore::PLUGIN_OK);
    check_result("gain 10", p->set_config("gain", "10"),
                 naadcore::PLUGIN_OK);
    check_eq("gain reads 10.000", p->get_config("gain"), "10.000");
    check_result("gain 10.1 out of range",
                 p->set_config("gain", "10.1"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("gain -0.1 out of range",
                 p->set_config("gain", "-0.1"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("gain not a number", p->set_config("gain", "abc"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("gain trailing junk", p->set_config("gain", "0.5x"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("gain nan rejected", p->set_config("gain", "nan"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("gain inf rejected", p->set_config("gain", "inf"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("gain empty rejected", p->set_config("gain", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("gain unchanged after rejects", p->get_config("gain"),
             "10.000");

    // live reverb / chorus
    check_result("reverb off", p->set_config("reverb", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("reverb reads off", p->get_config("reverb"), "off");
    check_result("reverb on", p->set_config("reverb", "on"),
                 naadcore::PLUGIN_OK);
    check_eq("reverb reads on", p->get_config("reverb"), "on");
    check_result("reverb bad value", p->set_config("reverb", "banana"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("chorus on", p->set_config("chorus", "on"),
                 naadcore::PLUGIN_OK);
    check_eq("chorus reads on", p->get_config("chorus"), "on");
    check_result("chorus off", p->set_config("chorus", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("chorus reads off", p->get_config("chorus"), "off");
    check_result("chorus bad value", p->set_config("chorus", "yes"),
                 naadcore::PLUGIN_INVALID_PARAM);

    // live envelope shaping (Phase 2: attack_ms / release_ms)
    check_result("attack_ms 25", p->set_config("attack_ms", "25"),
                 naadcore::PLUGIN_OK);
    check_eq("attack_ms reads 25", p->get_config("attack_ms"), "25");
    check_result("release_ms 350", p->set_config("release_ms", "350"),
                 naadcore::PLUGIN_OK);
    check_eq("release_ms reads 350", p->get_config("release_ms"), "350");
    // boundary values are accepted
    check_result("attack_ms 1 ok", p->set_config("attack_ms", "1"),
                 naadcore::PLUGIN_OK);
    check_result("attack_ms 2000 ok", p->set_config("attack_ms", "2000"),
                 naadcore::PLUGIN_OK);
    check_result("release_ms 1 ok", p->set_config("release_ms", "1"),
                 naadcore::PLUGIN_OK);
    check_result("release_ms 4000 ok", p->set_config("release_ms", "4000"),
                 naadcore::PLUGIN_OK);
    check_eq("release_ms reads 4000", p->get_config("release_ms"), "4000");
    // out of range rejected
    check_result("attack_ms 2001 out of range",
                 p->set_config("attack_ms", "2001"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("attack_ms 0 out of range",
                 p->set_config("attack_ms", "0"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("attack_ms negative",
                 p->set_config("attack_ms", "-5"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("release_ms 4001 out of range",
                 p->set_config("release_ms", "4001"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("release_ms 0 out of range",
                 p->set_config("release_ms", "0"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("release_ms negative",
                 p->set_config("release_ms", "-1"),
                 naadcore::PLUGIN_INVALID_PARAM);
    // junk rejected (integer keys also reject floats)
    check_result("attack_ms not a number",
                 p->set_config("attack_ms", "abc"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("attack_ms float rejected",
                 p->set_config("attack_ms", "10.5"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("attack_ms nan rejected",
                 p->set_config("attack_ms", "nan"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("attack_ms empty rejected",
                 p->set_config("attack_ms", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("release_ms not a number",
                 p->set_config("release_ms", "banana"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("release_ms float rejected",
                 p->set_config("release_ms", "12.5"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("release_ms inf rejected",
                 p->set_config("release_ms", "inf"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("release_ms empty rejected",
                 p->set_config("release_ms", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("attack_ms unchanged after rejects", p->get_config("attack_ms"),
             "2000");
    check_eq("release_ms unchanged after rejects", p->get_config("release_ms"),
             "4000");

    // pre-existing keys still work
    check_result("soundfont_path set", p->set_config("soundfont_path", "/x"),
                 naadcore::PLUGIN_OK);
    check_eq("soundfont_path reads", p->get_config("soundfont_path"), "/x");

    // reed stops (Phase 3: stop key, preset mapping single=0 / double=1)
    check_result("stop double", p->set_config("stop", "double"),
                 naadcore::PLUGIN_OK);
    check_eq("stop reads double", p->get_config("stop"), "double");
    check_result("stop single", p->set_config("stop", "single"),
                 naadcore::PLUGIN_OK);
    check_eq("stop reads single", p->get_config("stop"), "single");
    check_result("stop quad rejected", p->set_config("stop", "quad"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("stop case-sensitive rejected",
                 p->set_config("stop", "Double"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("stop trailing space rejected",
                 p->set_config("stop", "single "),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("stop empty rejected", p->set_config("stop", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("stop unchanged after rejects", p->get_config("stop"), "single");

    // layer router (Phase 4: coupler / sub_octave toggles, on/off strict)
    check_eq("coupler after init", p->get_config("coupler"), "off");
    check_eq("sub_octave after init", p->get_config("sub_octave"), "off");
    check_result("coupler on", p->set_config("coupler", "on"),
                 naadcore::PLUGIN_OK);
    check_eq("coupler reads on", p->get_config("coupler"), "on");
    check_result("sub_octave on", p->set_config("sub_octave", "on"),
                 naadcore::PLUGIN_OK);
    check_eq("sub_octave reads on", p->get_config("sub_octave"), "on");
    check_result("coupler off", p->set_config("coupler", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("coupler reads off", p->get_config("coupler"), "off");
    check_result("sub_octave off", p->set_config("sub_octave", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("sub_octave reads off", p->get_config("sub_octave"), "off");
    check_result("coupler bad value", p->set_config("coupler", "banana"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("coupler case-sensitive rejected",
                 p->set_config("coupler", "On"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("coupler trailing space rejected",
                 p->set_config("coupler", "on "),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("coupler empty rejected", p->set_config("coupler", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("sub_octave bad value",
                 p->set_config("sub_octave", "1"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("sub_octave case-sensitive rejected",
                 p->set_config("sub_octave", "OFF"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("sub_octave trailing space rejected",
                 p->set_config("sub_octave", "off "),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("sub_octave empty rejected",
                 p->set_config("sub_octave", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("coupler unchanged after rejects", p->get_config("coupler"),
             "off");
    check_eq("sub_octave unchanged after rejects",
             p->get_config("sub_octave"), "off");

    // unknown keys and null handling
    check_eq("unknown get is empty", p->get_config("bogus"), "");
    check_result("unknown set", p->set_config("bogus", "x"),
                 naadcore::PLUGIN_NOT_IMPLEMENTED);
    check_result("null key get-safe", p->set_config(nullptr, "x"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("null value", p->set_config("gain", nullptr),
                 naadcore::PLUGIN_INVALID_PARAM);

    // synth still plays notes after live config churn
    naadcore::MidiEvent ev{};
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 60;
    ev.data2 = 100;
    check_result("note on", p->handle_midi_event(ev), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    ev.data2 = 0;
    check_result("note off", p->handle_midi_event(ev), naadcore::PLUGIN_OK);

    // PROGRAM_CHANGE policy (Phase 3): incoming program changes are
    // ignored (stops are config-controlled). The event must be swallowed
    // without error; the preset is verified separately via plugin-in-loop
    // renders (a program change left the stop=single render untouched).
    ev.type = naadcore::MidiEvent::PROGRAM_CHANGE;
    ev.data1 = 1;
    check_result("program change ignored", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.data1 = 62;
    check_result("note on after program change", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("note off after program change", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);

    // ---- Phase 4 behavior, state-level (headless: only PLUGIN_OK and
    // config readback are observable here; audible verification is done
    // with plugin-in-loop renders — see tests/RESULTS.md Phase 4) ----

    // Mid-phrase layer toggles: set_config BETWEEN note events must not
    // disturb anything and the note-off still releases cleanly (the
    // layer voices started/released for the held note internally).
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 60;
    ev.data2 = 100;
    check_result("toggle: note on", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    check_result("toggle: coupler on mid-phrase",
                 p->set_config("coupler", "on"), naadcore::PLUGIN_OK);
    check_result("toggle: sub_octave on mid-phrase",
                 p->set_config("sub_octave", "on"), naadcore::PLUGIN_OK);
    check_result("toggle: coupler off mid-phrase",
                 p->set_config("coupler", "off"), naadcore::PLUGIN_OK);
    check_result("toggle: sub_octave off mid-phrase",
                 p->set_config("sub_octave", "off"), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("toggle: note off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);

    // Duplicate NoteOn while held: swallowed without error (no
    // re-trigger); a single off then releases the still-single voice.
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.data1 = 64;
    ev.data2 = 100;
    check_result("duplicate: note on", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.data2 = 40;
    check_result("duplicate: re-on while held ignored",
                 p->handle_midi_event(ev), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("duplicate: note off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.data1 = 67;
    ev.data2 = 90;
    check_result("duplicate: fresh note after dup sequence",
                 p->handle_midi_event(ev), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("duplicate: fresh note off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);

    // Cross-channel NoteOff: a held note must release no matter which
    // channel the off arrives on (bellows state is channel-agnostic).
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 60;
    ev.data2 = 100;
    check_result("cross-ch: note on ch0", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.channel = 3;
    ev.data1 = 64;
    ev.data2 = 80;
    check_result("cross-ch: note on ch3", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    ev.channel = 7;   // wrong channel for note 60 (started on ch0)
    ev.data1 = 60;
    check_result("cross-ch: note 60 off on ch7", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.channel = 1;   // wrong channel for note 64 (started on ch3)
    ev.data1 = 64;
    check_result("cross-ch: note 64 off on ch1", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);

    // CC 123 (All Notes Off): clears held state across all channels;
    // subsequent events must behave as after a full release.
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 5;
    ev.data1 = 72;
    ev.data2 = 100;
    check_result("cc123: note on ch5", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::CONTROL_CHANGE;
    ev.channel = 0;
    ev.data1 = 123;
    ev.data2 = 0;
    check_result("cc123: all notes off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 72;
    ev.data2 = 50;    // duplicate after CC123 must not error either way
    check_result("cc123: note on after cc123", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("cc123: note off after cc123", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);

    p->stop_audio();
    destroy(p);

    // config set before init is applied at init
    auto* q = create();
    check_result("pre-init gain 2.5", q->set_config("gain", "2.5"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init reverb off", q->set_config("reverb", "off"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init attack_ms 15", q->set_config("attack_ms", "15"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init release_ms 300", q->set_config("release_ms", "300"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init stop double", q->set_config("stop", "double"),
                 naadcore::PLUGIN_OK);
    check_eq("pre-init stop stored", q->get_config("stop"), "double");
    check_result("pre-init coupler on", q->set_config("coupler", "on"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init sub_octave on",
                 q->set_config("sub_octave", "on"), naadcore::PLUGIN_OK);
    check_eq("pre-init coupler stored", q->get_config("coupler"), "on");
    check_eq("pre-init sub_octave stored", q->get_config("sub_octave"),
             "on");
    check_result("q init", q->init(nullptr), naadcore::PLUGIN_OK);
    check_eq("pre-init gain applied", q->get_config("gain"), "2.500");
    check_eq("pre-init reverb applied", q->get_config("reverb"), "off");
    check_eq("pre-init attack_ms applied", q->get_config("attack_ms"), "15");
    check_eq("pre-init release_ms applied", q->get_config("release_ms"),
             "300");
    check_eq("pre-init stop applied", q->get_config("stop"), "double");
    check_eq("pre-init coupler applied", q->get_config("coupler"), "on");
    check_eq("pre-init sub_octave applied", q->get_config("sub_octave"),
             "on");
    // layers enabled from init: a note must start main + both layers and
    // release them all (headless: PLUGIN_OK + layer state readback)
    naadcore::MidiEvent evq{};
    evq.type = naadcore::MidiEvent::NOTE_ON;
    evq.channel = 2;
    evq.data1 = 60;
    evq.data2 = 100;
    check_result("layers: note on with both layers enabled",
                 q->handle_midi_event(evq), naadcore::PLUGIN_OK);
    evq.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("layers: note off releases all layers",
                 q->handle_midi_event(evq), naadcore::PLUGIN_OK);
    // a note whose octave coupler would exceed MIDI note 127 must not
    // error (the coupler voice is silently skipped)
    evq.type = naadcore::MidiEvent::NOTE_ON;
    evq.data1 = 120;
    evq.data2 = 100;
    check_result("layers: coupler clamp note on", q->handle_midi_event(evq),
                 naadcore::PLUGIN_OK);
    evq.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("layers: coupler clamp note off",
                 q->handle_midi_event(evq), naadcore::PLUGIN_OK);
    // live stop switch after init
    check_result("live stop single", q->set_config("stop", "single"),
                 naadcore::PLUGIN_OK);
    check_eq("live stop reads single", q->get_config("stop"), "single");
    check_result("live stop double", q->set_config("stop", "double"),
                 naadcore::PLUGIN_OK);
    check_eq("live stop reads double", q->get_config("stop"), "double");
    q->stop_audio();
    destroy(q);

    // Note: no dlclose() here. Fully unmapping libharmonium_plugin.so (and
    // with it FluidSynth/GLib) crashes in teardown on this system; the real
    // CLI never unmaps FluidSynth because naadcore_core links it too.
    std::cout << (failures ? "FAILED" : "PASSED") << ": "
              << (checks - failures) << "/" << checks << " checks" << std::endl;
    return failures ? 1 : 0;
}
