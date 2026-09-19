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
    check_eq("default audio_device", p->get_config("audio_device"), "");
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

    // ---- Audio output device selection (well-known "audio_device" key).
    // Headless: the fluidsettings write itself is init-time and is proven
    // by the CLI smoke runs (--audio-device with a real/bad device); here
    // we verify the seam: storage, echo, no-validation contract and
    // init-only semantics. ----

    // set/get round-trip (verbatim storage, machine-specific names)
    check_result("audio_device set", p->set_config("audio_device",
                                                   "plughw:CARD=PCH,DEV=0"),
                 naadcore::PLUGIN_OK);
    check_eq("audio_device reads", p->get_config("audio_device"),
             "plughw:CARD=PCH,DEV=0");

    // NO validation by design: junk strings accepted (never
    // PLUGIN_INVALID_PARAM) — the real validator is start_audio, where a
    // bad device fails driver creation
    check_result("audio_device junk accepted",
                 p->set_config("audio_device", "not_a_real_pcm !@#"),
                 naadcore::PLUGIN_OK);
    check_eq("audio_device junk echoes", p->get_config("audio_device"),
             "not_a_real_pcm !@#");

    // empty-string set normalizes to unset ("" echo = plugin default)
    check_result("audio_device empty accepted",
                 p->set_config("audio_device", ""), naadcore::PLUGIN_OK);
    check_eq("audio_device empty reads unset",
             p->get_config("audio_device"), "");
    // verbatim storage: even a lone space is a (weird) device name
    check_result("audio_device space accepted",
                 p->set_config("audio_device", " "), naadcore::PLUGIN_OK);
    check_eq("audio_device space echoes verbatim",
             p->get_config("audio_device"), " ");

    // post-init set is accepted (init-only semantics, like audio_driver):
    // no crash, echo reflects the stored value, running driver untouched
    check_result("audio_device post-init set accepted",
                 p->set_config("audio_device", "default"), naadcore::PLUGIN_OK);
    check_eq("audio_device post-init echo",
             p->get_config("audio_device"), "default");
    // restore unset for the note-flow checks below
    check_result("audio_device restore unset",
                 p->set_config("audio_device", ""), naadcore::PLUGIN_OK);
    check_eq("audio_device unset after restore",
             p->get_config("audio_device"), "");

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
    // with plugin-in-loop renders — see tests/RESULTS.md Phase 4 and the
    // coupler acoustic check in tests/scripts/check_coupler_acoustic.sh) —

    // Mid-phrase layer toggles: set_config BETWEEN note events must not
    // disturb anything and the note-off still releases cleanly.
    // COUPLER (user spec, 2026-09-19): a mid-hold toggle does NOT
    // retro-add/remove the octave voice on held notes — the new state
    // applies to NEW presses; every voice started at press time is
    // released at NoteOff (HeldNote.layers bookkeeping). SUB_OCTAVE keeps
    // the Phase 4 retro semantics (toggle starts/releases the layer for
    // held notes). At the state level both paths are PLUGIN_OK + clean
    // release; the coupler's no-retro semantics are proven acoustically.
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 60;
    ev.data2 = 100;
    check_result("toggle: note on", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    check_result("toggle: coupler on mid-phrase (new presses only)",
                 p->set_config("coupler", "on"), naadcore::PLUGIN_OK);
    check_result("toggle: sub_octave on mid-phrase (retro)",
                 p->set_config("sub_octave", "on"), naadcore::PLUGIN_OK);
    check_result("toggle: coupler off mid-phrase (held notes keep their "
                 "voices until NoteOff)",
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

    // ---- Phase 5: drone (fixture notes on internal channel 13) ----
    // Headless: start/stop of the drone voices is verified audibly with
    // plugin-in-loop renders (tests/RESULTS.md Phase 5); here we check
    // the config seam, the add/remove diff semantics (no errors, correct
    // readback, melody notes unaffected) and the CC 123 reset.

    // defaults
    check_eq("default drone", p->get_config("drone"), "off");
    check_eq("default drone_level", p->get_config("drone_level"), "45");

    // valid set/get: drone notes start immediately, melody above them
    // must keep working (drone never touches the bellows model)
    check_result("drone set 48,55", p->set_config("drone", "48,55"),
                 naadcore::PLUGIN_OK);
    check_eq("drone reads 48,55", p->get_config("drone"), "48,55");
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 69;
    ev.data2 = 100;
    check_result("drone: melody note over drone", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("drone: melody note off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);

    // add/remove semantics: "48,60" releases 55, starts 60, keeps 48
    // (state-level: no errors + readback; audible proof via T12 renders)
    check_result("drone 48,60 (release 55, start 60, keep 48)",
                 p->set_config("drone", "48,60"), naadcore::PLUGIN_OK);
    check_eq("drone reads 48,60", p->get_config("drone"), "48,60");
    // full stop: everything releases
    check_result("drone off", p->set_config("drone", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("drone reads off", p->get_config("drone"), "off");

    // empty string = off (canonical echo)
    check_result("drone set 48", p->set_config("drone", "48"),
                 naadcore::PLUGIN_OK);
    check_result("drone empty string = off", p->set_config("drone", ""),
                 naadcore::PLUGIN_OK);
    check_eq("drone empty reads off", p->get_config("drone"), "off");

    // boundary values accepted
    check_result("drone note 0 ok", p->set_config("drone", "0"),
                 naadcore::PLUGIN_OK);
    check_eq("drone note 0 reads", p->get_config("drone"), "0");
    check_result("drone note 127 ok", p->set_config("drone", "127"),
                 naadcore::PLUGIN_OK);
    check_eq("drone note 127 reads", p->get_config("drone"), "127");
    check_result("drone 8 notes ok (cap)",
                 p->set_config("drone", "48,50,52,55,57,59,60,62"),
                 naadcore::PLUGIN_OK);
    check_eq("drone 8 notes reads", p->get_config("drone"),
             "48,50,52,55,57,59,60,62");
    check_result("drone off again", p->set_config("drone", "off"),
                 naadcore::PLUGIN_OK);

    // strict validation: junk / range / format rejects leave state alone
    check_result("drone sargam names rejected", p->set_config("drone", "sa,pa"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone 128 out of range", p->set_config("drone", "128"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone negative rejected", p->set_config("drone", "-1"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone float token rejected", p->set_config("drone", "48.5"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone trailing comma rejected", p->set_config("drone", "48,"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone leading comma rejected", p->set_config("drone", ",48"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone empty token rejected",
                 p->set_config("drone", "48,,55"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone leading space rejected", p->set_config("drone", " 48"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone trailing space rejected", p->set_config("drone", "48 "),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone inner space rejected",
                 p->set_config("drone", "48, 55"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone case-sensitive off rejected",
                 p->set_config("drone", "OFF"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone keyword on rejected", p->set_config("drone", "on"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone overflow token rejected",
                 p->set_config("drone", "99999999999999999999999"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone duplicate note rejected",
                 p->set_config("drone", "48,48"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone 9 notes rejected",
                 p->set_config("drone", "36,40,43,45,48,50,52,55,57"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("drone unchanged after rejects", p->get_config("drone"), "off");

    // drone_level: 0..127 -> CC 7 on channel 13, live
    check_result("drone_level 60", p->set_config("drone_level", "60"),
                 naadcore::PLUGIN_OK);
    check_eq("drone_level reads 60", p->get_config("drone_level"), "60");
    check_result("drone_level 0 ok", p->set_config("drone_level", "0"),
                 naadcore::PLUGIN_OK);
    check_result("drone_level 127 ok", p->set_config("drone_level", "127"),
                 naadcore::PLUGIN_OK);
    check_eq("drone_level reads 127", p->get_config("drone_level"), "127");
    check_result("drone_level 128 rejected",
                 p->set_config("drone_level", "128"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone_level negative rejected",
                 p->set_config("drone_level", "-1"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone_level junk rejected",
                 p->set_config("drone_level", "loud"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone_level float rejected",
                 p->set_config("drone_level", "12.5"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone_level empty rejected",
                 p->set_config("drone_level", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("drone_level trailing space rejected",
                 p->set_config("drone_level", "45 "),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("drone_level unchanged after rejects",
             p->get_config("drone_level"), "127");
    check_result("drone_level restore 45", p->set_config("drone_level", "45"),
                 naadcore::PLUGIN_OK);

    // CC 123 is a full reset including the drone: voices silenced (all_
    // notes_off covers channel 13), spec kept in config, and re-issuing
    // the SAME spec restarts the notes (diff runs against sounding state)
    check_result("drone cc123: set 48,55", p->set_config("drone", "48,55"),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::CONTROL_CHANGE;
    ev.channel = 0;
    ev.data1 = 123;
    ev.data2 = 0;
    check_result("drone cc123: all notes off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    check_eq("drone cc123: spec kept in config", p->get_config("drone"),
             "48,55");
    check_result("drone cc123: restart same spec",
                 p->set_config("drone", "48,55"), naadcore::PLUGIN_OK);
    check_eq("drone cc123: reads after restart", p->get_config("drone"),
             "48,55");
    check_result("drone cc123: off after restart", p->set_config("drone", "off"),
                 naadcore::PLUGIN_OK);

    // ---- Phase 6: key_click (faint chiff layer on internal ch 12) and
    // variation (per-note velocity micro-variation). Headless: the audible
    // proof (click transient levels, self-end, non-identical repeats) is
    // done with plugin-in-loop renders — see tests/RESULTS.md Phase 6.
    // Here: config seam, strict validation, next-NoteOn semantics and
    // note-flow integrity with both features active.

    // defaults
    check_eq("default key_click", p->get_config("key_click"), "off");
    check_eq("default variation", p->get_config("variation"), "on");

    // valid set/get (mode is consulted per accepted NoteOn — a live toggle
    // applies from the next NoteOn; state-level proof below)
    check_result("key_click low", p->set_config("key_click", "low"),
                 naadcore::PLUGIN_OK);
    check_eq("key_click reads low", p->get_config("key_click"), "low");
    check_result("key_click high", p->set_config("key_click", "high"),
                 naadcore::PLUGIN_OK);
    check_eq("key_click reads high", p->get_config("key_click"), "high");
    check_result("key_click off", p->set_config("key_click", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("key_click reads off", p->get_config("key_click"), "off");
    // strict validation: junk / case / whitespace / empty rejected, state kept
    check_result("key_click junk rejected",
                 p->set_config("key_click", "medium"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("key_click case-sensitive rejected",
                 p->set_config("key_click", "LOW"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("key_click trailing space rejected",
                 p->set_config("key_click", "low "),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("key_click empty rejected",
                 p->set_config("key_click", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("key_click unchanged after rejects", p->get_config("key_click"),
             "off");

    // notes play cleanly with the click enabled; mid-phrase toggle between
    // events (next-NoteOn semantics at state level)
    check_result("key_click: set low", p->set_config("key_click", "low"),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 60;
    ev.data2 = 100;
    check_result("key_click: note on with click", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    check_result("key_click: toggle high mid-note",
                 p->set_config("key_click", "high"), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("key_click: note off with click",
                 p->handle_midi_event(ev), naadcore::PLUGIN_OK);

    // duplicate NoteOn while held with the click ON: still swallowed (a
    // swallowed duplicate makes NO click — no pallet moved), one release
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.data1 = 64;
    ev.data2 = 100;
    check_result("key_click: note on", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.data2 = 40;
    check_result("key_click: duplicate ignored (no click)",
                 p->handle_midi_event(ev), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("key_click: note off after duplicate",
                 p->handle_midi_event(ev), naadcore::PLUGIN_OK);

    // variation: on/off live keys, strict validation
    check_result("variation off", p->set_config("variation", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("variation reads off", p->get_config("variation"), "off");
    check_result("variation on", p->set_config("variation", "on"),
                 naadcore::PLUGIN_OK);
    check_eq("variation reads on", p->get_config("variation"), "on");
    check_result("variation junk rejected", p->set_config("variation", "1"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("variation case-sensitive rejected",
                 p->set_config("variation", "OFF"),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("variation trailing space rejected",
                 p->set_config("variation", "off "),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_result("variation empty rejected", p->set_config("variation", ""),
                 naadcore::PLUGIN_INVALID_PARAM);
    check_eq("variation unchanged after rejects", p->get_config("variation"),
             "on");

    // repeated notes with variation on and off (state level: no errors,
    // clean releases; the audible jitter difference is proven by renders)
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 0;
    ev.data1 = 67;
    ev.data2 = 100;
    for (int i = 0; i < 2; ++i) {
        check_result("variation: repeated note on", p->handle_midi_event(ev),
                     naadcore::PLUGIN_OK);
        ev.type = naadcore::MidiEvent::NOTE_OFF;
        check_result("variation: repeated note off", p->handle_midi_event(ev),
                     naadcore::PLUGIN_OK);
        ev.type = naadcore::MidiEvent::NOTE_ON;
    }
    check_result("variation: set off", p->set_config("variation", "off"),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_ON;
    check_result("variation off: note on", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("variation off: note off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);

    // CC 123 with the click on: full reset still clean (a sounding click
    // voice is simply cut short; no click state exists to clear)
    check_result("key_click: re-set low", p->set_config("key_click", "low"),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.data1 = 72;
    ev.data2 = 100;
    check_result("key_click cc123: note on", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::CONTROL_CHANGE;
    ev.data1 = 123;
    check_result("key_click cc123: all notes off", p->handle_midi_event(ev),
                 naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.data1 = 74;
    ev.data2 = 90;
    check_result("key_click cc123: note after reset",
                 p->handle_midi_event(ev), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("key_click cc123: note off after reset",
                 p->handle_midi_event(ev), naadcore::PLUGIN_OK);
    check_result("key_click: off again", p->set_config("key_click", "off"),
                 naadcore::PLUGIN_OK);

    p->stop_audio();
    destroy(p);

    // config set before init is applied at init (Phase 6 keys included)
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
    check_result("pre-init drone 48,55", q->set_config("drone", "48,55"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init drone_level 50", q->set_config("drone_level", "50"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init key_click high", q->set_config("key_click", "high"),
                 naadcore::PLUGIN_OK);
    check_result("pre-init variation off", q->set_config("variation", "off"),
                 naadcore::PLUGIN_OK);
    // audio_device: pre-init storage (the path PluginManager::load_plugin
    // uses — it must be accepted before init and echo after it)
    check_result("pre-init audio_device", q->set_config("audio_device",
                                                        "default"),
                 naadcore::PLUGIN_OK);
    check_eq("pre-init audio_device stored", q->get_config("audio_device"),
             "default");
    check_eq("pre-init drone stored", q->get_config("drone"), "48,55");
    check_eq("pre-init drone_level stored", q->get_config("drone_level"),
             "50");
    check_eq("pre-init key_click stored", q->get_config("key_click"), "high");
    check_eq("pre-init variation stored", q->get_config("variation"), "off");
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
    check_eq("pre-init drone applied", q->get_config("drone"), "48,55");
    check_eq("pre-init drone_level applied", q->get_config("drone_level"),
             "50");
    check_eq("pre-init key_click applied", q->get_config("key_click"),
             "high");
    check_eq("pre-init variation applied", q->get_config("variation"), "off");
    check_eq("pre-init audio_device applied", q->get_config("audio_device"),
             "default");
    // init-time per-driver mapping is proven by the smoke runs; at the
    // seam level the stored value must survive init unchanged
    // (alsa/default: "audio.alsa.device" was set from it during init).
    // Post-init set accepted (init-only): stores for the next run only.
    check_result("audio_device post-init live set",
                 q->set_config("audio_device", "hw:CARD=PCH,DEV=0"),
                 naadcore::PLUGIN_OK);
    check_eq("audio_device post-init live echo",
             q->get_config("audio_device"), "hw:CARD=PCH,DEV=0");
    check_result("audio_device post-init live unset",
                 q->set_config("audio_device", ""), naadcore::PLUGIN_OK);
    check_eq("audio_device post-init live unset reads",
             q->get_config("audio_device"), "");
    // live Phase 6 changes after a pre-init config
    check_result("pre-init key_click live low", q->set_config("key_click", "low"),
                 naadcore::PLUGIN_OK);
    check_eq("pre-init key_click live reads", q->get_config("key_click"),
             "low");
    check_result("pre-init variation live on", q->set_config("variation", "on"),
                 naadcore::PLUGIN_OK);
    check_eq("pre-init variation live reads", q->get_config("variation"),
             "on");
    // live drone changes after a pre-init config: remove 55, then off
    check_result("pre-init drone live trim", q->set_config("drone", "48"),
                 naadcore::PLUGIN_OK);
    check_eq("pre-init drone live trim reads", q->get_config("drone"), "48");
    check_result("pre-init drone live off", q->set_config("drone", "off"),
                 naadcore::PLUGIN_OK);
    check_eq("pre-init drone live off reads", q->get_config("drone"), "off");
    check_result("pre-init drone_level live", q->set_config("drone_level", "45"),
                 naadcore::PLUGIN_OK);
    // a note with key_click=low + variation=on (both live after pre-init)
    ev.type = naadcore::MidiEvent::NOTE_ON;
    ev.channel = 2;
    ev.data1 = 61;
    ev.data2 = 100;
    check_result("pre-init click+variation note on",
                 q->handle_midi_event(ev), naadcore::PLUGIN_OK);
    ev.type = naadcore::MidiEvent::NOTE_OFF;
    check_result("pre-init click+variation note off",
                 q->handle_midi_event(ev), naadcore::PLUGIN_OK);
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
