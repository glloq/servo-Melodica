#ifndef PLATFORM_CALIBRATIONCONSOLE_H
#define PLATFORM_CALIBRATIONCONSOLE_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "Config.h"
#include "ConfigStore.h"
#include "ConfigValidator.h"
#include "Defaults.h"
#include "IKeyDriver.h"

namespace melodica {

// Serial calibration mode integrated into the unified firmware (no separate
// sketch to flash). It edits the live BoardConfig — the key driver reads
// calibration on every press, so edits take effect immediately — and can save to
// NVS or export a C++/JSON snapshot. Fully non-blocking: update() consumes at
// most the bytes already buffered on the serial port.
//
// Commands (one per line):
//   k <n>     select key n            r <us>   set rest pulse
//   p <us>    set pressed pulse       lo <us>  set min limit
//   hi <us>   set max limit           v        toggle inversion
//   t         test selected key       ta       test all keys
//   s         save to NVS             d        reset selected key to default
//   x         export as C++           j        export as JSON
//   val       validate the config     ?        help
//
// Key indices are bounded by the RUNTIME key count (instrument.noteCount, up to
// MAX_NOTES = 64), not by the compiled default of 32.
class CalibrationConsole {
public:
    CalibrationConsole(BoardConfig& cfg, IKeyDriver& keys, Stream& io)
        : cfg_(cfg), keys_(keys), io_(io) {}

    void begin() {
        io_.println(F("\n== Calibration console == (type ? for help)"));
        printSelected();
    }

    void update() {
        while (io_.available() > 0) {
            char c = static_cast<char>(io_.read());
            if (c == '\n' || c == '\r') {
                if (len_ > 0) { line_[len_] = 0; handle(line_); len_ = 0; }
            } else if (len_ < sizeof(line_) - 1) {
                line_[len_++] = c;
            }
        }
    }

private:
    void handle(const char* line) {
        char cmd[8] = {0};
        long arg = 0;
        int n = sscanf(line, "%7s %ld", cmd, &arg);
        if (n < 1) return;

        if (!strcmp(cmd, "?")) { help(); return; }
        if (!strcmp(cmd, "k")) { sel_ = clampIdx(arg); printSelected(); return; }
        if (!strcmp(cmd, "r")) { cfg_.keys[sel_].restPulseUs = arg; apply(); return; }
        if (!strcmp(cmd, "p")) { cfg_.keys[sel_].pressedPulseUs = arg; apply(); return; }
        if (!strcmp(cmd, "lo")) { cfg_.keys[sel_].minPulseUs = arg; apply(); return; }
        if (!strcmp(cmd, "hi")) { cfg_.keys[sel_].maxPulseUs = arg; apply(); return; }
        if (!strcmp(cmd, "v")) { cfg_.keys[sel_].inverted = !cfg_.keys[sel_].inverted; apply(); return; }
        if (!strcmp(cmd, "t")) { testKey(sel_); return; }
        if (!strcmp(cmd, "ta")) { for (uint16_t i = 0; i < activeKeys(); ++i) testKey(i); return; }
        if (!strcmp(cmd, "val")) { validate(); return; }
        if (!strcmp(cmd, "d")) { cfg_.keys[sel_] = makeDefaultConfig().keys[sel_]; apply(); return; }
        if (!strcmp(cmd, "s")) {
            io_.println(ConfigStore::save(cfg_) ? F("saved to NVS") : F("SAVE FAILED"));
            return;
        }
        if (!strcmp(cmd, "x")) { exportCpp(); return; }
        if (!strcmp(cmd, "j")) { exportJson(); return; }
        // Air-system configuration (works on every profile, incl. non-WiFi).
        if (!strcmp(cmd, "air")) {
            cfg_.air.type = (AirSystemType)arg;
            io_.print(F("air type=")); io_.print((int)arg);
            io_.println(F(" (save with 's', reboot to apply)"));
            return;
        }
        if (!strcmp(cmd, "pol")) {
            cfg_.air.policy = (AirPolicy)arg;
            io_.print(F("air policy=")); io_.println((int)arg);
            return;
        }
        if (!strcmp(cmd, "reboot")) {
            io_.println(F("rebooting..."));
            delay(100);
            ESP.restart();
            return;
        }
        io_.println(F("unknown command (type ?)"));
    }

    // The number of keys this instrument actually has, from the runtime config.
    uint16_t activeKeys() const {
        uint16_t n = cfg_.instrument.noteCount;
        if (n > MAX_NOTES) n = MAX_NOTES;
        return n == 0 ? 1 : n;
    }

    uint16_t clampIdx(long v) {
        if (v < 0) return 0;
        if (v >= static_cast<long>(activeKeys())) return static_cast<uint16_t>(activeKeys() - 1);
        return static_cast<uint16_t>(v);
    }

    void validate() {
        ConfigValidator::Result r = ConfigValidator::validate(cfg_);
        for (uint8_t i = 0; i < r.count; ++i) {
            io_.print(r[i].severity == IssueSeverity::Error ? F("ERROR   ") : F("warning "));
            io_.println(r[i].text);
        }
        io_.print(r.errors);
        io_.print(F(" error(s), "));
        io_.print(r.warnings);
        io_.println(F(" warning(s)"));
    }

    void apply() {
        // Re-seat the key at its (new) rest position so the change is visible.
        keys_.releaseKey(sel_);
        keys_.update(micros());
        printSelected();
    }

    void testKey(uint16_t i) {
        io_.print(F("test key ")); io_.println(i);
        keys_.pressKey(i);
        keys_.update(micros());
        delay(250);  // calibration-mode only: safe to pause, no MIDI is flowing
        keys_.releaseKey(i);
        keys_.update(micros());
        delay(150);
    }

    void printSelected() {
        const KeyCalibration& k = cfg_.keys[sel_];
        io_.print(F("key ")); io_.print(sel_);
        io_.print(F(" rest=")); io_.print(k.restPulseUs);
        io_.print(F(" pressed=")); io_.print(k.pressedPulseUs);
        io_.print(F(" min=")); io_.print(k.minPulseUs);
        io_.print(F(" max=")); io_.print(k.maxPulseUs);
        io_.print(F(" inv=")); io_.println(k.inverted ? 1 : 0);
    }

    void exportCpp() {
        io_.print(F("const KeyCalibration kCalib["));
        io_.print(activeKeys());
        io_.println(F("] = {"));
        for (uint16_t i = 0; i < activeKeys(); ++i) {
            const KeyCalibration& k = cfg_.keys[i];
            io_.print(F("  {")); io_.print(k.restPulseUs); io_.print(F(", "));
            io_.print(k.pressedPulseUs); io_.print(F(", "));
            io_.print(k.minPulseUs); io_.print(F(", "));
            io_.print(k.maxPulseUs); io_.print(F(", "));
            io_.print(k.inverted ? F("true") : F("false")); io_.println(F("},"));
        }
        io_.println(F("};"));
    }

    void exportJson() {
        io_.print(F("{\"keys\":["));
        for (uint16_t i = 0; i < activeKeys(); ++i) {
            const KeyCalibration& k = cfg_.keys[i];
            io_.print(F("{\"rest\":")); io_.print(k.restPulseUs);
            io_.print(F(",\"pressed\":")); io_.print(k.pressedPulseUs);
            io_.print(F(",\"min\":")); io_.print(k.minPulseUs);
            io_.print(F(",\"max\":")); io_.print(k.maxPulseUs);
            io_.print(F(",\"inv\":")); io_.print(k.inverted ? 1 : 0);
            io_.print(i + 1 < activeKeys() ? F("},") : F("}"));
        }
        io_.println(F("]}"));
    }

    void help() {
        io_.println(F("keys: k<n> r<us> p<us> lo<us> hi<us> v t ta d s x j"));
        io_.println(F("air:  air<0-10 type> pol<0-4>  (then s + reboot)"));
        io_.println(F("misc: val (validate config) reboot"));
        io_.print(F("keys available: 0.."));
        io_.println(activeKeys() - 1);
    }

    BoardConfig& cfg_;
    IKeyDriver& keys_;
    Stream& io_;
    uint16_t sel_ = 0;
    char line_[48];
    uint8_t len_ = 0;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // PLATFORM_CALIBRATIONCONSOLE_H
