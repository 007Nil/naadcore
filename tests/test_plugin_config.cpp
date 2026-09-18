// Config-seam tests for the harmonium plugin (gain/reverb/chorus and
// Phase 2 envelope attack_ms/release_ms live keys).
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
    check_result("q init", q->init(nullptr), naadcore::PLUGIN_OK);
    check_eq("pre-init gain applied", q->get_config("gain"), "2.500");
    check_eq("pre-init reverb applied", q->get_config("reverb"), "off");
    check_eq("pre-init attack_ms applied", q->get_config("attack_ms"), "15");
    check_eq("pre-init release_ms applied", q->get_config("release_ms"),
             "300");
    q->stop_audio();
    destroy(q);

    // Note: no dlclose() here. Fully unmapping libharmonium_plugin.so (and
    // with it FluidSynth/GLib) crashes in teardown on this system; the real
    // CLI never unmaps FluidSynth because naadcore_core links it too.
    std::cout << (failures ? "FAILED" : "PASSED") << ": "
              << (checks - failures) << "/" << checks << " checks" << std::endl;
    return failures ? 1 : 0;
}
