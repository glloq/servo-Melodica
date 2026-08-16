#ifndef PLATFORM_WEBCONFIGPORTAL_H
#define PLATFORM_WEBCONFIGPORTAL_H

// Web-based configuration UI, compiled only for profiles that already use WiFi
// (MELODICA_WEB_CONFIG). It lets the user configure EVERYTHING from a browser:
// network credentials, MIDI filter (including the per-channel list), the
// air-system type with all its parameters and pins, servo speed, I2C and safety
// limits, and per-key calibration — then validates it and saves to NVS.
//
// Access:
//   * If the device is joined to a network (STA), the page is served at its IP.
//   * If it is not configured, or the operator long-presses BOOT, the device
//     starts a WiFi hotspot ("ServoMelodica-Setup") with a captive portal and
//     serves the page at http://192.168.4.1/.
//
// The hotspot is brought up through NetworkManager, which owns the WiFi mode, so
// the RTP-MIDI transport's reconnect attempts can no longer tear it down while
// somebody is configuring the instrument.
//
// Uses only the Arduino-ESP32 built-in WebServer + DNSServer (no extra libs), so
// it compiles unchanged in the Arduino IDE. All handling is non-blocking.
#if defined(ARDUINO) && defined(MELODICA_WEB_CONFIG)

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "Config.h"
#include "ConfigStore.h"
#include "ConfigValidator.h"
#include "IAirController.h"
#include "IKeyDriver.h"
#include "IMidiTransport.h"
#include "InstrumentController.h"
#include "Log.h"
#include "NetworkManager.h"
#include "SafetyManager.h"

namespace melodica {

class WebConfigPortal {
public:
    // `keys` (optional) enables live key testing / calibration preview from the UI.
    WebConfigPortal(BoardConfig& cfg, IKeyDriver* keys = nullptr)
        : cfg_(cfg), keys_(keys), server_(80) {}

    // Optional live-status sources for the dashboard (/api/status).
    void attachStatus(SafetyManager* safety, InstrumentController* instrument,
                      IMidiTransport* transport, IAirController* air) {
        safety_ = safety;
        instrument_ = instrument;
        transport_ = transport;
        air_ = air;
    }

    // Register routes and start the HTTP server (works on STA and/or AP).
    void begin() {
        server_.on("/", HTTP_GET, [this]() { handleRoot(); });
        server_.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
        server_.on("/api/key", HTTP_GET, [this]() { handleGetKey(); });
        server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
        server_.on("/api/validate", HTTP_GET, [this]() { handleValidate(); });
        server_.on("/save", HTTP_POST, [this]() { handleSave(); });
        server_.on("/calibrate", HTTP_POST, [this]() { handleCalibrate(); });
        server_.on("/testkey", HTTP_POST, [this]() { handleTestKey(); });
        server_.on("/action", HTTP_POST, [this]() { handleAction(); });
        server_.onNotFound([this]() { handleRoot(); });  // captive-portal catch-all
        server_.begin();
        LOG_INFO("Web config server started");
    }

    // Bring up the configuration hotspot (AP) with a captive DNS.
    void startHotspot(const char* apSsid = "ServoMelodica-Setup") {
        if (apActive_) return;
        if (!NetworkManager::instance().startAccessPoint(apSsid)) return;
        IPAddress ip = NetworkManager::instance().apIp();
        dns_.start(53, "*", ip);
        apActive_ = true;
        LOG_INFO("Hotspot '%s' up, config at http://%s/", apSsid, ip.toString().c_str());
    }

    bool hotspotActive() const { return apActive_; }

    // Pump the server + captive DNS; handle a pending reboot. Non-blocking.
    void update() {
        if (apActive_) dns_.processNextRequest();
        server_.handleClient();
        if (rebootRequested_ && millis() >= rebootAtMs_) {
            LOG_WARN("Rebooting to apply configuration");
            ESP.restart();
        }
    }

private:
    // Read an integer form field, falling back to the current value if absent.
    long argI(const char* name, long cur) {
        return server_.hasArg(name) ? server_.arg(name).toInt() : cur;
    }
    bool argB(const char* name) { return server_.hasArg(name) && server_.arg(name) == "1"; }

    // Number of keys this instrument actually has (runtime geometry, not the
    // compiled default): everything key-indexed is bounded by this.
    uint16_t activeKeys() const {
        uint16_t n = cfg_.instrument.noteCount;
        return n > MAX_NOTES ? MAX_NOTES : n;
    }

    void scheduleReboot() {
        rebootRequested_ = true;
        rebootAtMs_ = millis() + 800;  // let the HTTP response flush first
    }

    void handleGetConfig() {
        const auto& a = cfg_.air;
        const auto& m = cfg_.midi;
        const auto& n = cfg_.network;
        const auto& s = cfg_.safety;
        String j = "{";
        j += "\"ssid\":\"" + String(n.ssid) + "\",";
        j += "\"session\":\"" + String(n.sessionName) + "\",";
        j += "\"reconnectBase\":" + String(n.reconnectBaseMs) + ",";
        j += "\"reconnectMax\":" + String(n.reconnectMaxMs) + ",";
        j += "\"midiMode\":" + String((int)m.channelMode) + ",";
        j += "\"midiCh\":" + String(m.singleChannel) + ",";
        j += "\"chmask\":" + String(m.channelMask) + ",";
        j += "\"transpose\":" + String(m.transpose) + ",";
        j += "\"lowNote\":" + String(m.lowNote) + ",";
        j += "\"highNote\":" + String(m.highNote) + ",";
        j += "\"airType\":" + String((int)a.type) + ",";
        j += "\"airPolicy\":" + String((int)a.policy) + ",";
        j += "\"airInv\":" + String(a.inverted ? 1 : 0) + ",";
        j += "\"airStart\":" + String(a.startThreshold) + ",";
        j += "\"airMin\":" + String(a.minOutput) + ",";
        j += "\"airMax\":" + String(a.maxOutput) + ",";
        j += "\"attack\":" + String(a.attackRampMs) + ",";
        j += "\"release\":" + String(a.releaseRampMs) + ",";
        j += "\"precharge\":" + String(a.precharge) + ",";
        j += "\"pwmFreq\":" + String(a.pwmFrequencyHz) + ",";
        j += "\"airPreMs\":" + String(a.airPrechargeMs) + ",";
        j += "\"keyPressMs\":" + String(a.keyPressDelayMs) + ",";
        j += "\"keyRelMs\":" + String(a.keyReleaseDelayMs) + ",";
        j += "\"airRelMs\":" + String(a.airReleaseDelayMs) + ",";
        j += "\"pinPrimary\":" + String(a.wiring.primaryPin) + ",";
        j += "\"pinSecondary\":" + String(a.wiring.secondaryPin) + ",";
        j += "\"pinEnable\":" + String(a.wiring.enablePin) + ",";
        j += "\"forward\":" + String(a.wiring.forward ? 1 : 0) + ",";
        j += "\"ledcCh\":" + String(a.wiring.ledcChannel) + ",";
        j += "\"ledcBits\":" + String(a.wiring.ledcResolutionBits) + ",";
        j += "\"stage0\":" + String(a.wiring.stagePins[0]) + ",";
        j += "\"stage1\":" + String(a.wiring.stagePins[1]) + ",";
        j += "\"stage2\":" + String(a.wiring.stagePins[2]) + ",";
        j += "\"stage3\":" + String(a.wiring.stagePins[3]) + ",";
        j += "\"stageCount\":" + String(a.wiring.stageCount) + ",";
        j += "\"stepPin\":" + String(a.wiring.stepPin) + ",";
        j += "\"dirPin\":" + String(a.wiring.dirPin) + ",";
        j += "\"stepperMax\":" + String(a.wiring.stepperMaxSteps) + ",";
        // Instrument geometry
        j += "\"noteCount\":" + String(cfg_.instrument.noteCount) + ",";
        j += "\"firstNote\":" + String(cfg_.instrument.firstMidiNote) + ",";
        // I2C / PCA9685
        j += "\"boardCount\":" + String(cfg_.keyDriver.boardCount) + ",";
        j += "\"sda\":" + String(cfg_.i2c.sdaPin) + ",";
        j += "\"scl\":" + String(cfg_.i2c.sclPin) + ",";
        j += "\"i2cHz\":" + String(cfg_.i2c.clockHz) + ",";
        j += "\"oePin\":" + String(cfg_.keyDriver.oePin) + ",";
        j += "\"pcaHz\":" + String(cfg_.keyDriver.pwmFrequencyHz) + ",";
        j += "\"slew\":" + String(cfg_.keyDriver.slewUsPerMs) + ",";
        j += "\"healthMs\":" + String(cfg_.keyDriver.healthCheckIntervalMs) + ",";
        j += "\"pca0\":" + String(cfg_.keyDriver.addresses[0]) + ",";
        j += "\"pca1\":" + String(cfg_.keyDriver.addresses[1]) + ",";
        j += "\"pca2\":" + String(cfg_.keyDriver.addresses[2]) + ",";
        j += "\"pca3\":" + String(cfg_.keyDriver.addresses[3]) + ",";
        // Composite air stages
        const auto& co = a.composite;
        j += "\"cMainValve\":" + String((int)co.mainValve) + ",";
        j += "\"cFlow\":" + String((int)co.flow) + ",";
        j += "\"cSource\":" + String((int)co.source) + ",";
        j += "\"cMainPin\":" + String(co.mainValvePin) + ",";
        j += "\"cMainAH\":" + String(co.mainValveActiveHigh ? 1 : 0) + ",";
        j += "\"cFlowPin\":" + String(co.flowPin) + ",";
        j += "\"cFlowCh\":" + String(co.flowLedcChannel) + ",";
        j += "\"cSrcPwm\":" + String(co.sourcePwmPin) + ",";
        j += "\"cSrcDir\":" + String(co.sourceDirPin) + ",";
        j += "\"cSrcEn\":" + String(co.sourceEnablePin) + ",";
        j += "\"cSrcCh\":" + String(co.sourceLedcChannel) + ",";
        j += "\"cFwd\":" + String(co.sourceForward ? 1 : 0) + ",";
        j += "\"cSensEn\":" + String(co.sensorEnabled ? 1 : 0) + ",";
        j += "\"cSensPin\":" + String(co.sensorPin) + ",";
        j += "\"cSensMin\":" + String(co.sensorMinRaw) + ",";
        j += "\"cSensMax\":" + String(co.sensorMaxRaw) + ",";
        j += "\"cTarget\":" + String(co.targetPressure) + ",";
        j += "\"cHyst\":" + String(co.pressureHysteresis) + ",";
        // Safety
        j += "\"commMs\":" + String(s.commTimeoutMs) + ",";
        j += "\"keyHoldMs\":" + String(s.maxKeyHoldMs) + ",";
        j += "\"pumpMs\":" + String(s.maxPumpRunMs) + ",";
        j += "\"idleOff\":" + String(s.disableOutputsOnIdle ? 1 : 0) + ",";
        j += "\"idleMs\":" + String(s.idleDisableDelayMs) + ",";
        j += "\"senseMs\":" + String(s.activeSensingTimeoutMs) + ",";
        j += "\"panicPin\":" + String(s.panicPin) + ",";
        j += "\"panicHigh\":" + String(s.panicActiveHigh ? 1 : 0) + ",";
        j += "\"maxNotes\":" + String(MAX_NOTES);
        j += "}";
        server_.send(200, "application/json", j);
    }

    // Per-key calibration readback.
    //
    // Without this the calibration panel showed the same hard-coded 1500/1900 for
    // every key, so selecting key 12 and pressing "save" overwrote whatever that
    // key had actually been calibrated to. The UI now loads the real values every
    // time the key selector changes.
    void handleGetKey() {
        long idx = argI("index", -1);
        if (idx < 0 || idx >= static_cast<long>(activeKeys())) {
            server_.send(400, "application/json", "{\"error\":\"bad key index\"}");
            return;
        }
        const KeyCalibration& k = cfg_.keys[idx];
        String j = "{";
        j += "\"index\":" + String((int)idx) + ",";
        j += "\"rest\":" + String(k.restPulseUs) + ",";
        j += "\"pressed\":" + String(k.pressedPulseUs) + ",";
        j += "\"min\":" + String(k.minPulseUs) + ",";
        j += "\"max\":" + String(k.maxPulseUs) + ",";
        j += "\"inverted\":" + String(k.inverted ? 1 : 0) + ",";
        j += "\"note\":" + String(cfg_.instrument.firstMidiNote + (int)idx) + ",";
        j += "\"count\":" + String(activeKeys());
        j += "}";
        server_.send(200, "application/json", j);
    }

    // Live status: what the instrument is doing and why it stopped, if it did.
    void handleStatus() {
        String j = "{";
        j += "\"uptimeMs\":" + String(millis()) + ",";
        j += "\"heap\":" + String((uint32_t)ESP.getFreeHeap()) + ",";
        j += "\"ap\":" + String(apActive_ ? 1 : 0) + ",";
        j += "\"ip\":\"" + NetworkManager::instance().stationIp().toString() + "\",";
        j += "\"apIp\":\"" + NetworkManager::instance().apIp().toString() + "\",";
        if (transport_) {
            j += "\"transport\":\"" + String(transport_->name()) + "\",";
            j += "\"connected\":" + String(transport_->connected() ? 1 : 0) + ",";
        } else {
            j += "\"transport\":\"n/a\",\"connected\":0,";
        }
        j += "\"fault\":\"" + String(safety_ ? safety_->faultText() : "n/a") + "\",";
        j += "\"inFault\":" + String((safety_ && safety_->inFault()) ? 1 : 0) + ",";
        j += "\"activeKeys\":" + String(instrument_ ? instrument_->activeKeyCount() : 0) + ",";
        j += "\"keysHealthy\":" + String((keys_ && keys_->healthy()) ? 1 : 0) + ",";
        if (air_) {
            j += "\"airRunning\":" + String(air_->isRunning() ? 1 : 0) + ",";
            j += "\"airRuntimeMs\":" + String(air_->runtimeMs()) + ",";
            j += "\"airHealthy\":" + String(air_->healthy() ? 1 : 0) + ",";
            j += "\"airFault\":\"" + String(airFaultText(air_->fault())) + "\"";
        } else {
            j += "\"airRunning\":0,\"airRuntimeMs\":0,\"airHealthy\":1,\"airFault\":\"n/a\"";
        }
        j += "}";
        server_.send(200, "application/json", j);
    }

    void handleValidate() {
        server_.send(200, "application/json", validationJson(ConfigValidator::validate(cfg_)));
    }

    // Takes the Result rather than the config: a Result is ~1.2 kB of stack and
    // a caller that already has one must not pay for a second.
    static String validationJson(const ConfigValidator::Result& r) {
        String j = "{\"errors\":" + String(r.errors) + ",\"warnings\":" + String(r.warnings) +
                   ",\"issues\":[";
        for (uint8_t i = 0; i < r.count; ++i) {
            if (i) j += ",";
            j += "{\"sev\":\"";
            j += (r[i].severity == IssueSeverity::Error ? "error" : "warning");
            j += "\",\"text\":\"";
            j += escape(r[i].text);
            j += "\"}";
        }
        j += "]}";
        return j;
    }

    static String escape(const char* s) {
        String out;
        for (const char* p = s; *p; ++p) {
            if (*p == '"' || *p == '\\') out += '\\';
            out += *p;
        }
        return out;
    }

    void handleSave() {
        // Apply the form to a COPY, validate it, and only then commit: a
        // configuration that cannot work on this hardware is reported back
        // instead of being saved and rebooted into.
        BoardConfig next = cfg_;
        auto& a = next.air;
        auto& m = next.midi;
        auto& n = next.network;
        auto& s = next.safety;

        if (server_.hasArg("ssid")) strlcpy(n.ssid, server_.arg("ssid").c_str(), sizeof(n.ssid));
        if (server_.hasArg("pass") && server_.arg("pass").length() > 0)
            strlcpy(n.password, server_.arg("pass").c_str(), sizeof(n.password));
        if (server_.hasArg("session"))
            strlcpy(n.sessionName, server_.arg("session").c_str(), sizeof(n.sessionName));
        n.reconnectBaseMs = (uint16_t)argI("reconnectBase", n.reconnectBaseMs);
        n.reconnectMaxMs = (uint16_t)argI("reconnectMax", n.reconnectMaxMs);

        m.channelMode = (ChannelMode)argI("midiMode", (long)m.channelMode);
        m.singleChannel = (uint8_t)argI("midiCh", m.singleChannel);
        m.channelMask = (uint16_t)argI("chmask", m.channelMask);
        m.transpose = (int8_t)argI("transpose", m.transpose);
        m.lowNote = (uint8_t)argI("lowNote", m.lowNote);
        m.highNote = (uint8_t)argI("highNote", m.highNote);

        a.type = (AirSystemType)argI("airType", (long)a.type);
        a.policy = (AirPolicy)argI("airPolicy", (long)a.policy);
        a.inverted = argB("airInv");
        a.startThreshold = (uint8_t)argI("airStart", a.startThreshold);
        a.minOutput = (uint8_t)argI("airMin", a.minOutput);
        a.maxOutput = (uint8_t)argI("airMax", a.maxOutput);
        a.attackRampMs = (uint16_t)argI("attack", a.attackRampMs);
        a.releaseRampMs = (uint16_t)argI("release", a.releaseRampMs);
        a.precharge = (uint8_t)argI("precharge", a.precharge);
        a.pwmFrequencyHz = (uint32_t)argI("pwmFreq", a.pwmFrequencyHz);
        a.airPrechargeMs = (uint16_t)argI("airPreMs", a.airPrechargeMs);
        a.keyPressDelayMs = (uint16_t)argI("keyPressMs", a.keyPressDelayMs);
        a.keyReleaseDelayMs = (uint16_t)argI("keyRelMs", a.keyReleaseDelayMs);
        a.airReleaseDelayMs = (uint16_t)argI("airRelMs", a.airReleaseDelayMs);
        a.wiring.primaryPin = (int8_t)argI("pinPrimary", a.wiring.primaryPin);
        a.wiring.secondaryPin = (int8_t)argI("pinSecondary", a.wiring.secondaryPin);
        a.wiring.enablePin = (int8_t)argI("pinEnable", a.wiring.enablePin);
        a.wiring.forward = argB("forward");
        a.wiring.ledcChannel = (uint8_t)argI("ledcCh", a.wiring.ledcChannel);
        a.wiring.ledcResolutionBits = (uint8_t)argI("ledcBits", a.wiring.ledcResolutionBits);
        a.wiring.stagePins[0] = (int8_t)argI("stage0", a.wiring.stagePins[0]);
        a.wiring.stagePins[1] = (int8_t)argI("stage1", a.wiring.stagePins[1]);
        a.wiring.stagePins[2] = (int8_t)argI("stage2", a.wiring.stagePins[2]);
        a.wiring.stagePins[3] = (int8_t)argI("stage3", a.wiring.stagePins[3]);
        a.wiring.stageCount = (uint8_t)argI("stageCount", a.wiring.stageCount);
        a.wiring.stepPin = (int8_t)argI("stepPin", a.wiring.stepPin);
        a.wiring.dirPin = (int8_t)argI("dirPin", a.wiring.dirPin);
        a.wiring.stepperMaxSteps = (uint16_t)argI("stepperMax", a.wiring.stepperMaxSteps);

        // Instrument geometry (clamped to the compiled cap).
        long nc = argI("noteCount", next.instrument.noteCount);
        if (nc < 1) nc = 1;
        if (nc > MAX_NOTES) nc = MAX_NOTES;
        next.instrument.noteCount = (uint8_t)nc;
        next.instrument.firstMidiNote = (uint8_t)argI("firstNote", next.instrument.firstMidiNote);

        // I2C / PCA9685
        long bc = argI("boardCount", next.keyDriver.boardCount);
        if (bc < 1) bc = 1;
        if (bc > MAX_PCA_BOARDS) bc = MAX_PCA_BOARDS;
        next.keyDriver.boardCount = (uint8_t)bc;
        next.i2c.sdaPin = (int8_t)argI("sda", next.i2c.sdaPin);
        next.i2c.sclPin = (int8_t)argI("scl", next.i2c.sclPin);
        next.i2c.clockHz = (uint32_t)argI("i2cHz", next.i2c.clockHz);
        next.keyDriver.oePin = (int8_t)argI("oePin", next.keyDriver.oePin);
        next.keyDriver.pwmFrequencyHz = (uint16_t)argI("pcaHz", next.keyDriver.pwmFrequencyHz);
        next.keyDriver.slewUsPerMs = (uint16_t)argI("slew", next.keyDriver.slewUsPerMs);
        next.keyDriver.healthCheckIntervalMs =
            (uint16_t)argI("healthMs", next.keyDriver.healthCheckIntervalMs);
        next.keyDriver.addresses[0] = (uint8_t)argI("pca0", next.keyDriver.addresses[0]);
        next.keyDriver.addresses[1] = (uint8_t)argI("pca1", next.keyDriver.addresses[1]);
        next.keyDriver.addresses[2] = (uint8_t)argI("pca2", next.keyDriver.addresses[2]);
        next.keyDriver.addresses[3] = (uint8_t)argI("pca3", next.keyDriver.addresses[3]);

        // Composite air stages
        CompositeAirConfig& co = a.composite;
        co.mainValve = (MainValveKind)argI("cMainValve", (long)co.mainValve);
        co.flow = (FlowKind)argI("cFlow", (long)co.flow);
        co.source = (SourceKind)argI("cSource", (long)co.source);
        co.mainValvePin = (int8_t)argI("cMainPin", co.mainValvePin);
        co.mainValveActiveHigh = argB("cMainAH");
        co.flowPin = (int8_t)argI("cFlowPin", co.flowPin);
        co.flowLedcChannel = (uint8_t)argI("cFlowCh", co.flowLedcChannel);
        co.sourcePwmPin = (int8_t)argI("cSrcPwm", co.sourcePwmPin);
        co.sourceDirPin = (int8_t)argI("cSrcDir", co.sourceDirPin);
        co.sourceEnablePin = (int8_t)argI("cSrcEn", co.sourceEnablePin);
        co.sourceLedcChannel = (uint8_t)argI("cSrcCh", co.sourceLedcChannel);
        co.sourceForward = argB("cFwd");
        co.sensorEnabled = argB("cSensEn");
        co.sensorPin = (int8_t)argI("cSensPin", co.sensorPin);
        co.sensorMinRaw = (uint16_t)argI("cSensMin", co.sensorMinRaw);
        co.sensorMaxRaw = (uint16_t)argI("cSensMax", co.sensorMaxRaw);
        co.targetPressure = (uint8_t)argI("cTarget", co.targetPressure);
        co.pressureHysteresis = (uint8_t)argI("cHyst", co.pressureHysteresis);

        // Safety
        s.commTimeoutMs = (uint32_t)argI("commMs", s.commTimeoutMs);
        s.maxKeyHoldMs = (uint32_t)argI("keyHoldMs", s.maxKeyHoldMs);
        s.maxPumpRunMs = (uint32_t)argI("pumpMs", s.maxPumpRunMs);
        s.disableOutputsOnIdle = argB("idleOff");
        s.idleDisableDelayMs = (uint32_t)argI("idleMs", s.idleDisableDelayMs);
        s.activeSensingTimeoutMs = (uint16_t)argI("senseMs", s.activeSensingTimeoutMs);
        s.panicPin = (int8_t)argI("panicPin", s.panicPin);
        s.panicActiveHigh = argB("panicHigh");

        ConfigValidator::Result check = ConfigValidator::validate(next);
        if (!check.ok() && !argB("force")) {
            // 409: the form is understood, the resulting machine is not buildable.
            server_.send(409, "application/json", validationJson(check));
            return;
        }

        cfg_ = next;
        bool ok = ConfigStore::save(cfg_);
        server_.send(ok ? 200 : 500, "text/plain",
                     ok ? "Saved. Rebooting to apply..." : "Save failed");
        if (ok) scheduleReboot();
    }

    void handleCalibrate() {
        long idx = argI("key", -1);
        // Bounded by the RUNTIME key count: a 64-key instrument can calibrate
        // keys 32..63, which the compiled NUMBER_OF_NOTES used to reject.
        if (idx < 0 || idx >= static_cast<long>(activeKeys())) {
            server_.send(400, "text/plain", "bad key index");
            return;
        }
        KeyCalibration k = cfg_.keys[idx];
        k.restPulseUs = (uint16_t)argI("rest", k.restPulseUs);
        k.pressedPulseUs = (uint16_t)argI("pressed", k.pressedPulseUs);
        k.minPulseUs = (uint16_t)argI("min", k.minPulseUs);
        k.maxPulseUs = (uint16_t)argI("max", k.maxPulseUs);
        k.inverted = argB("inv");
        if (k.minPulseUs >= k.maxPulseUs) {
            server_.send(400, "text/plain", "min pulse must be below max pulse");
            return;
        }
        cfg_.keys[idx] = k;
        // Apply live so the change is visible on the servo immediately.
        if (keys_) { keys_->releaseKey(idx); keys_->update(micros()); }
        bool ok = ConfigStore::save(cfg_);
        server_.send(ok ? 200 : 500, "text/plain", ok ? "calibration saved" : "save failed");
    }

    // Momentarily press or release a key so the operator can calibrate by eye.
    void handleTestKey() {
        long idx = argI("key", -1);
        if (!keys_ || idx < 0 || idx >= static_cast<long>(activeKeys())) {
            server_.send(400, "text/plain", "no key driver / bad index");
            return;
        }
        if (server_.arg("state") == "press") keys_->pressKey(idx);
        else keys_->releaseKey(idx);
        keys_->update(micros());
        server_.send(200, "text/plain", "ok");
    }

    void handleAction() {
        String cmd = server_.arg("cmd");
        if (cmd == "reboot") {
            server_.send(200, "text/plain", "rebooting");
            scheduleReboot();
        } else if (cmd == "factory") {
            ConfigStore::factoryReset();
            server_.send(200, "text/plain", "factory reset, rebooting");
            scheduleReboot();
        } else if (cmd == "clearfault") {
            if (safety_) {
                safety_->clearFault();
                server_.send(200, "text/plain", "fault cleared");
            } else {
                server_.send(400, "text/plain", "no safety manager");
            }
        } else if (cmd == "panic") {
            if (safety_) {
                safety_->triggerPanic(micros());
                server_.send(200, "text/plain", "PANIC: instrument stopped");
            } else {
                server_.send(400, "text/plain", "no safety manager");
            }
        } else {
            server_.send(400, "text/plain", "unknown cmd");
        }
    }

    void handleRoot() { server_.send_P(200, "text/html", kPage); }

    BoardConfig& cfg_;
    IKeyDriver* keys_;
    WebServer server_;
    DNSServer dns_;
    SafetyManager* safety_ = nullptr;
    InstrumentController* instrument_ = nullptr;
    IMidiTransport* transport_ = nullptr;
    IAirController* air_ = nullptr;
    bool apActive_ = false;
    bool rebootRequested_ = false;
    uint32_t rebootAtMs_ = 0;

    static const char kPage[] PROGMEM;
};

// Self-contained configuration page. Loads current values from /api/config,
// per-key calibration from /api/key, live state from /api/status, and posts
// changes back as a URL-encoded form.
inline const char WebConfigPortal::kPage[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>Servo Melodica - Config</title>
<style>body{font-family:sans-serif;max-width:640px;margin:0 auto;padding:12px;background:#111;color:#eee}
h1{font-size:1.2em}fieldset{border:1px solid #444;margin:8px 0;border-radius:8px}
label{display:block;margin:6px 0 2px}input,select{width:100%;padding:6px;box-sizing:border-box;background:#222;color:#eee;border:1px solid #555;border-radius:4px}
button{padding:10px;margin:6px 4px 0 0;border:0;border-radius:6px;background:#2a7;color:#fff;font-weight:bold}
button.warn{background:#a33}.row{display:flex;gap:8px}.row>div{flex:1}small{color:#aaa}
#status{background:#181818;border:1px solid #333;border-radius:8px;padding:8px;font-size:.9em}
.err{color:#f77}.warn2{color:#fc6}.ok{color:#7d7}
.chs{display:flex;flex-wrap:wrap}.chs label{width:25%;margin:2px 0}.chs input{width:auto;margin-right:4px}</style></head>
<body><h1>Servo Melodica</h1>
<div id=status>loading status...</div>
<div id=issues></div>
<form id=f method=post action=/save>
<fieldset><legend>Network (WiFi / RTP-MIDI)</legend>
<label>SSID<input name=ssid></label>
<label>Password <small>(leave blank to keep)</small><input name=pass type=password></label>
<label>RTP-MIDI session name<input name=session maxlength=23></label>
<div class=row><div><label>Reconnect base ms<input name=reconnectBase type=number></label></div>
<div><label>Reconnect max ms<input name=reconnectMax type=number></label></div></div></fieldset>
<fieldset><legend>MIDI</legend>
<label>Channel mode<select name=midiMode><option value=0>Single</option><option value=1>List</option><option value=2>Omni</option></select></label>
<div class=row><div><label>Single channel <small>(mode Single)</small><input name=midiCh type=number min=0 max=15></label></div>
<div><label>Transpose<input name=transpose type=number min=-64 max=64></label></div></div>
<b>Accepted channels <small>(mode List)</small></b><div class=chs id=chs></div>
<input type=hidden name=chmask id=chmask>
<div class=row><div><label>Low note<input name=lowNote type=number min=0 max=127></label></div>
<div><label>High note<input name=highNote type=number min=0 max=127></label></div></div></fieldset>
<fieldset><legend>Melodica geometry</legend>
<div class=row><div><label>Number of keys<input name=noteCount type=number min=1 max=64></label></div>
<div><label>Lowest MIDI note<input name=firstNote type=number min=0 max=127></label></div></div>
<small>Up to 64 keys (4 PCA9685). Default 32 keys from F4 (65).</small></fieldset>
<fieldset><legend>I2C / PCA9685 boards</legend>
<div class=row><div><label>Board count<input name=boardCount type=number min=1 max=4></label></div>
<div><label>SDA pin<input name=sda type=number></label></div>
<div><label>SCL pin<input name=scl type=number></label></div>
<div><label>OE pin (-1=none)<input name=oePin type=number></label></div></div>
<div class=row><div><label>PCA #0 addr<input name=pca0 type=number></label></div>
<div><label>#1<input name=pca1 type=number></label></div>
<div><label>#2<input name=pca2 type=number></label></div>
<div><label>#3<input name=pca3 type=number></label></div></div>
<div class=row><div><label>I2C clock Hz<input name=i2cHz type=number></label></div>
<div><label>Servo PWM Hz<input name=pcaHz type=number></label></div></div>
<div class=row><div><label>Servo speed us/ms <small>(0 = instant)</small><input name=slew type=number min=0></label></div>
<div><label>I2C health check ms<input name=healthMs type=number min=0></label></div></div>
<small>Addresses are decimal (0x40 = 64). "Servo speed" limits how fast a key travels.</small></fieldset>
<fieldset><legend>Air system</legend>
<label>Type<select name=airType>
<option value=0>Servo valve</option><option value=1>Solenoid</option><option value=2>Stepped solenoids</option>
<option value=3>Blower PWM</option><option value=4>Pump H-bridge</option><option value=5>PWM valve</option>
<option value=6>DAC valve</option><option value=7>Stepper bellows</option><option value=8>External air</option>
<option value=9>Simulation</option><option value=10>Composite (multi-stage)</option></select></label>
<label>Flow policy<select name=airPolicy><option value=0>Fixed</option><option value=1>Max velocity</option>
<option value=2>Average velocity</option><option value=3>Sum clamped</option><option value=4>Polyphony comp.</option></select></label>
<label><input type=checkbox name=airInv value=1 style=width:auto> Invert output</label>
<div class=row><div><label>Start threshold<input name=airStart type=number></label></div>
<div><label>Min output<input name=airMin type=number></label></div>
<div><label>Max output<input name=airMax type=number></label></div></div>
<div class=row><div><label>Attack ms<input name=attack type=number></label></div>
<div><label>Release ms<input name=release type=number></label></div>
<div><label>Precharge<input name=precharge type=number></label></div></div>
<div class=row><div><label>PWM frequency Hz<input name=pwmFreq type=number></label></div>
<div><label>LEDC channel<input name=ledcCh type=number min=0 max=15></label></div>
<div><label>LEDC bits<input name=ledcBits type=number min=1 max=14></label></div></div>
<div class=row><div><label>Air precharge ms<input name=airPreMs type=number></label></div>
<div><label>Key press delay ms<input name=keyPressMs type=number></label></div></div>
<div class=row><div><label>Key release delay ms<input name=keyRelMs type=number></label></div>
<div><label>Air release delay ms<input name=airRelMs type=number></label></div></div>
<b>Pins</b>
<div class=row><div><label>Primary<input name=pinPrimary type=number></label></div>
<div><label>Secondary<input name=pinSecondary type=number></label></div>
<div><label>Enable<input name=pinEnable type=number></label></div></div>
<label><input type=checkbox name=forward value=1 style=width:auto> Pump/bellows forward</label>
<div class=row><div><label>Step pin<input name=stepPin type=number></label></div>
<div><label>Dir pin<input name=dirPin type=number></label></div>
<div><label>Stepper max<input name=stepperMax type=number></label></div></div>
<b>Stepped solenoids</b>
<div class=row><div><label>Stage count<input name=stageCount type=number min=1 max=4></label></div>
<div><label>Stage 0<input name=stage0 type=number></label></div>
<div><label>Stage 1<input name=stage1 type=number></label></div></div>
<div class=row><div><label>Stage 2<input name=stage2 type=number></label></div>
<div><label>Stage 3<input name=stage3 type=number></label></div></div></fieldset>
<fieldset><legend>Composite air stages <small>(when air type = Composite)</small></legend>
<label>Main valve<select name=cMainValve><option value=0>None (pass-through)</option><option value=1>Solenoid</option></select></label>
<div class=row><div><label>Main valve pin<input name=cMainPin type=number></label></div>
<div><label><input type=checkbox name=cMainAH value=1 style=width:auto> Active high</label></div></div>
<label>Flow (servoflow)<select name=cFlow><option value=0>None</option><option value=1>Servo</option><option value=2>PWM</option><option value=3>DAC</option></select></label>
<div class=row><div><label>Flow pin<input name=cFlowPin type=number></label></div>
<div><label>Flow LEDC ch<input name=cFlowCh type=number min=0 max=15></label></div></div>
<label>Source<select name=cSource><option value=0>External air</option><option value=1>Blower/fan</option><option value=2>Pump (H-bridge)</option><option value=3>Reservoir (regulated)</option></select></label>
<div class=row><div><label>Source PWM pin<input name=cSrcPwm type=number></label></div>
<div><label>Dir pin<input name=cSrcDir type=number></label></div>
<div><label>Enable pin<input name=cSrcEn type=number></label></div>
<div><label>Source LEDC ch<input name=cSrcCh type=number min=0 max=15></label></div></div>
<label><input type=checkbox name=cFwd value=1 style=width:auto> Pump forward</label>
<b>Pressure sensor / reservoir</b>
<label><input type=checkbox name=cSensEn value=1 style=width:auto> Sensor enabled</label>
<div class=row><div><label>Sensor pin<input name=cSensPin type=number></label></div>
<div><label>Raw min<input name=cSensMin type=number></label></div>
<div><label>Raw max<input name=cSensMax type=number></label></div></div>
<div class=row><div><label>Target pressure<input name=cTarget type=number min=0 max=255></label></div>
<div><label>Hysteresis<input name=cHyst type=number min=0 max=255></label></div></div>
<small>A "Reservoir" source requires a valid sensor: without one the pump is refused.</small></fieldset>
<fieldset><legend>Safety</legend>
<div class=row><div><label>Comm timeout ms<input name=commMs type=number></label></div>
<div><label>Max key hold ms<input name=keyHoldMs type=number></label></div></div>
<div class=row><div><label>Max pump run ms <small>(0 = off)</small><input name=pumpMs type=number></label></div>
<div><label>Active sensing ms<input name=senseMs type=number></label></div></div>
<label><input type=checkbox name=idleOff value=1 style=width:auto> Disable servo outputs when idle</label>
<label>Idle delay ms<input name=idleMs type=number></label>
<div class=row><div><label>Emergency stop pin (-1=none)<input name=panicPin type=number></label></div>
<div><label><input type=checkbox name=panicHigh value=1 style=width:auto> Stop when pin reads HIGH</label></div></div>
<small>Recommended: normally-closed button to GND, "stop when HIGH" checked, so a cut wire also stops the instrument.</small></fieldset>
<button type=submit>Save &amp; reboot</button>
</form>
<fieldset><legend>Calibration (single key)</legend>
<div class=row><div><label>Key<input id=cKey type=number value=0 min=0></label></div>
<div><label>Rest us<input id=cRest type=number></label></div>
<div><label>Pressed us<input id=cPressed type=number></label></div></div>
<div class=row><div><label>Min us<input id=cMin type=number></label></div>
<div><label>Max us<input id=cMax type=number></label></div>
<div><label><input id=cInv type=checkbox style=width:auto> Invert</label></div></div>
<small id=cInfo></small><br>
<button onclick=saveCal() type=button>Save key calibration</button>
<button onclick=testKey('press') type=button>Press</button>
<button onclick=testKey('release') type=button>Release</button></fieldset>
<button class=warn onclick=act('panic') type=button>STOP (panic)</button>
<button onclick=act('clearfault') type=button>Clear fault</button>
<button class=warn onclick=act('factory') type=button>Factory reset</button>
<button onclick=act('reboot') type=button>Reboot</button>
<p id=msg></p>
<script>
let CH=document.getElementById('chs');
for(let i=0;i<16;i++){CH.insertAdjacentHTML('beforeend','<label><input type=checkbox id=ch'+i+'>Ch'+(i+1)+'</label>');}
async function load(){let c=await(await fetch('/api/config')).json();
 for(let k in c){let e=document.querySelector('[name='+k+']');if(e){if(e.type=='checkbox')e.checked=c[k]==1;else e.value=c[k];}}
 for(let i=0;i<16;i++)document.getElementById('ch'+i).checked=((c.chmask>>i)&1)==1;
 document.getElementById('cKey').max=c.noteCount-1;loadKey();validate();}
function mask(){let m=0;for(let i=0;i<16;i++)if(document.getElementById('ch'+i).checked)m|=(1<<i);return m;}
async function loadKey(){let i=document.getElementById('cKey').value;
 let r=await fetch('/api/key?index='+i);if(!r.ok){cInfo.textContent='no such key';return;}
 let k=await r.json();cRest.value=k.rest;cPressed.value=k.pressed;cMin.value=k.min;cMax.value=k.max;cInv.checked=k.inverted==1;
 cInfo.textContent='key '+k.index+' of '+k.count+', MIDI note '+k.note;}
document.getElementById('cKey').addEventListener('change',loadKey);
async function validate(){let v=await(await fetch('/api/validate')).json();render(v);}
function render(v){let h='';
 if(v.errors==0&&v.warnings==0)h='<p class=ok>Configuration looks consistent.</p>';
 for(let it of v.issues)h+='<p class="'+(it.sev=='error'?'err':'warn2')+'">'+(it.sev=='error'?'&#9888; ':'&#9888; ')+it.text+'</p>';
 document.getElementById('issues').innerHTML=h;}
async function status(){try{let s=await(await fetch('/api/status')).json();
 document.getElementById('status').innerHTML=
 '<b class="'+(s.inFault?'err':'ok')+'">'+s.fault+'</b> &middot; '+s.transport+' '+(s.connected?'connected':'offline')+
 ' &middot; keys '+s.activeKeys+(s.keysHealthy?'':' <span class=err>(I2C fault)</span>')+
 ' &middot; air '+(s.airRunning?('running '+Math.round(s.airRuntimeMs/1000)+'s'):'idle')+
 (s.airHealthy?'':' <span class=err>('+s.airFault+')</span>')+
 '<br><small>ip '+s.ip+(s.ap?' / hotspot '+s.apIp:'')+' &middot; up '+Math.round(s.uptimeMs/1000)+'s &middot; heap '+s.heap+'</small>';
 }catch(e){}}
function post(u,b){return fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});}
function act(cmd){post('/action','cmd='+cmd).then(r=>r.text()).then(t=>{msg.textContent=t;status();});}
function saveCal(){let b='key='+cKey.value+'&rest='+cRest.value+'&pressed='+cPressed.value+'&min='+cMin.value+'&max='+cMax.value+'&inv='+(cInv.checked?1:0);
 post('/calibrate',b).then(r=>r.text()).then(t=>msg.textContent=t);}
function testKey(st){post('/testkey','key='+cKey.value+'&state='+st).then(r=>r.text()).then(t=>msg.textContent='key '+cKey.value+' '+st);}
document.getElementById('f').addEventListener('submit',async ev=>{ev.preventDefault();
 document.getElementById('chmask').value=mask();
 let body=new URLSearchParams(new FormData(ev.target)).toString();
 let r=await post('/save',body);
 if(r.status==409){let v=await r.json();render(v);
  msg.innerHTML='Not saved: '+v.errors+' error(s). <button type=button onclick="force()">Save anyway</button>';
  window.force=async()=>{let r2=await post('/save',body+'&force=1');msg.textContent=await r2.text();};
  window.scrollTo(0,0);return;}
 msg.textContent=await r.text();});
load();status();setInterval(status,2000);
</script></body></html>)HTML";

}  // namespace melodica

#endif  // ARDUINO && MELODICA_WEB_CONFIG
#endif  // PLATFORM_WEBCONFIGPORTAL_H
