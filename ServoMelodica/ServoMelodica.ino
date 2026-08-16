// ===========================================================================
//  Servo Melodica — unified firmware composition root
// ===========================================================================
//  One source tree builds every profile. It compiles unchanged in BOTH the
//  Arduino IDE (open ServoMelodica.ino) and PlatformIO.
//
//  * Transport is chosen at compile time (build flag, or UserConfig.h in the
//    Arduino IDE): TRANSPORT_BLE | TRANSPORT_WIFI_RTP | TRANSPORT_DIN |
//    TRANSPORT_USB | TRANSPORT_SERIAL.
//  * The AIR SYSTEM is chosen at RUNTIME from the stored config (web UI / serial
//    console). By default every air module is compiled in; a -DAIR_<X> flag
//    trims the binary to a single module.
//  * On WiFi profiles a web config UI + captive hotspot is available: a LONG
//    press on the BOOT button (or an unconfigured network) starts the hotspot.
//  * SAFETY: the BOOT button is a CONFIGURATION control (short press = local
//    panic, long press = hotspot). A real emergency stop is a dedicated input
//    (safety.panicPin), acted upon the instant it changes state — see below.
// ===========================================================================
#include <Arduino.h>

#include "UserConfig.h"  // compile-time defaults (transport) — edit in Arduino IDE

#include "AirControllerFactory.h"
#include "Config.h"
#include "ConfigValidator.h"
#include "Defaults.h"
#include "InstrumentController.h"
#include "Log.h"
#include "PanicInput.h"
#include "Pca9685KeyDriver.h"
#include "SafetyManager.h"
#include "CalibrationConsole.h"
#include "ConfigStore.h"

// The web config portal is available on WiFi builds.
#if defined(MELODICA_WEB_CONFIG)
#include "WebConfigPortal.h"
#endif

// --- Transport selection ---------------------------------------------------
#if defined(TRANSPORT_BLE)
#include <BLEMIDI_Transport.h>
#if defined(MELODICA_BLE_NIMBLE)
#include <hardware/BLEMIDI_ESP32_NimBLE.h>
#else
#include <hardware/BLEMIDI_ESP32.h>
#endif
#include "BleMidiTransport.h"
BLEMIDI_CREATE_INSTANCE("Servo Melodica", MIDI);
#elif defined(TRANSPORT_WIFI_RTP)
#include <AppleMIDI.h>
#include <WiFi.h>
#include "NetworkManager.h"
#include "RtpMidiTransport.h"
APPLEMIDI_CREATE_INSTANCE(WiFiUDP, MIDI, "Servo Melodica", DEFAULT_CONTROL_PORT);
#elif defined(TRANSPORT_DIN)
#include "DinMidiTransport.h"
#elif defined(TRANSPORT_USB)
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "UsbMidiTransport.h"
Adafruit_USBD_MIDI gUsbMidiDev;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, gUsbMidiDev, MIDI);
#elif defined(TRANSPORT_SERIAL)
#include "SerialMidiTransport.h"
#else
#error "Select a transport: TRANSPORT_BLE / WIFI_RTP / DIN / USB / SERIAL"
#endif

using namespace melodica;

namespace {
constexpr int8_t kConfigButtonPin = 0;  // BOOT button, active-low
constexpr bool kEnableCalibrationConsole = true;
constexpr uint32_t kLongPressMs = 3000;   // >= this => start config hotspot
constexpr uint32_t kShortPressMaxMs = 1500;  // <= this (on release) => panic
}  // namespace

// Runtime configuration + component pointers (bound in setup()).
static BoardConfig gConfig;
static IKeyDriver* gKeys = nullptr;
static IAirController* gAir = nullptr;
static InstrumentController* gInstrument = nullptr;
static SafetyManager* gSafety = nullptr;
static IMidiTransport* gTransport = nullptr;
static CalibrationConsole* gConsole = nullptr;
static PanicInput gPanicInput;
static bool gWasConnected = false;
#if defined(MELODICA_WEB_CONFIG)
static WebConfigPortal* gPortal = nullptr;
#endif

// Report what the stored configuration cannot do on this board. This runs on
// every boot, including on profiles with no web UI, so a mistake made over the
// serial console is not silent either.
static void reportConfigIssues() {
    ConfigValidator::Result r = ConfigValidator::validate(gConfig);
    for (uint8_t i = 0; i < r.count; ++i) {
        if (r[i].severity == IssueSeverity::Error) {
            LOG_ERROR("config: %s", r[i].text);
        } else {
            LOG_WARN("config: %s", r[i].text);
        }
    }
    if (r.errors > 0) {
        LOG_ERROR("Configuration has %u error(s): the affected hardware stays disabled",
                  (unsigned)r.errors);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    LOG_INFO("Servo Melodica unified firmware starting");

    gConfig = ConfigStore::load();
    reportConfigIssues();

    // --- Key driver (PCA9685 x N) ---
    static Pca9685KeyDriver keys(gConfig.i2c, gConfig.keyDriver, gConfig.keys,
                                 gConfig.instrument.noteCount);
    gKeys = &keys;

    // --- Air system: selected at RUNTIME from gConfig.air.type (incl. the
    //     4-stage Composite: source -> main valve -> flow -> optional sensor) ---
    static AirControllerStorage airStorage;
    static CompositeAirStorage compositeStorage;
    gAir = createAirController(gConfig, airStorage, compositeStorage);

    static InstrumentController instrument(gConfig, keys, *gAir);
    gInstrument = &instrument;

    static SafetyManager safety(gConfig.safety, instrument, keys, *gAir);
    gSafety = &safety;

    // --- Transport (exactly one) ---
#if defined(TRANSPORT_BLE)
    static BleMidiTransport<decltype(MIDI), decltype(BLEMIDI)> transport(MIDI, BLEMIDI);
#elif defined(TRANSPORT_WIFI_RTP)
    // NetworkManager owns the WiFi mode; the transport only asks for a station.
    NetworkManager::instance().begin();
    static RtpMidiTransport<decltype(MIDI), decltype(AppleMIDI)> transport(
        MIDI, AppleMIDI, gConfig.network.ssid, gConfig.network.password,
        gConfig.network.reconnectBaseMs, gConfig.network.reconnectMaxMs,
        gConfig.network.sessionName);
#elif defined(TRANSPORT_DIN)
    static DinMidiTransport transport(Serial2, 16, 17, gConfig.safety.activeSensingTimeoutMs);
#elif defined(TRANSPORT_USB)
    static UsbMidiTransport<decltype(MIDI)> transport(MIDI);
#elif defined(TRANSPORT_SERIAL)
    static SerialMidiTransport transport(Serial2, 115200, 16, 17,
                                         gConfig.safety.activeSensingTimeoutMs);
#endif
    gTransport = &transport;

    pinMode(kConfigButtonPin, INPUT_PULLUP);
    // Dedicated emergency stop, if one is wired. INPUT_PULLUP suits the
    // recommended normally-closed button to GND.
    if (gConfig.safety.panicPin >= 0) {
        pinMode(gConfig.safety.panicPin, INPUT_PULLUP);
        LOG_INFO("Emergency stop on GPIO%d (active %s)", (int)gConfig.safety.panicPin,
                 gConfig.safety.panicActiveHigh ? "HIGH" : "LOW");
    } else {
        LOG_WARN("No dedicated emergency-stop input configured");
    }

    if (!instrument.begin()) {
        LOG_ERROR("Instrument init reported a hardware problem (continuing safely)");
    }
    transport.begin();
    LOG_INFO("Transport: %s", transport.name());

#if defined(MELODICA_WEB_CONFIG)
    static WebConfigPortal portal(gConfig, &keys);
    gPortal = &portal;
    portal.attachStatus(&safety, &instrument, &transport, gAir);
    portal.begin();
    // No network credentials yet => bring up the setup hotspot automatically.
    if (gConfig.network.ssid[0] == '\0') {
        LOG_INFO("No WiFi credentials stored: starting config hotspot");
        portal.startHotspot();
    }
#endif

    if (kEnableCalibrationConsole) {
        static CalibrationConsole console(gConfig, keys, Serial);
        gConsole = &console;
        console.begin();
    }
}

// Dedicated emergency stop: acts on the TRANSITION into the active state, while
// the button is still held, and latches a Panic fault that only an explicit
// clear (web UI / console) releases.
static void serviceEmergencyStop(uint32_t nowUs) {
    if (gConfig.safety.panicPin < 0) return;
    const bool level = digitalRead(gConfig.safety.panicPin) == HIGH;
    const bool activeNow = gConfig.safety.panicActiveHigh ? level : !level;
    if (gPanicInput.update(millis(), activeNow, gConfig.safety.panicDebounceMs)) {
        LOG_ERROR("EMERGENCY STOP asserted on GPIO%d", (int)gConfig.safety.panicPin);
        gSafety->triggerPanic(nowUs);
    }
}

// Distinguish a short BOOT press (local panic) from a long press (start hotspot).
static void serviceConfigButton(uint32_t nowUs) {
    static bool down = false;
    static uint32_t startMs = 0;
    static bool longHandled = false;
    const uint32_t nowMs = millis();
    const bool pressed = digitalRead(kConfigButtonPin) == LOW;

    if (pressed && !down) {
        down = true;
        startMs = nowMs;
        longHandled = false;
    } else if (pressed && down) {
        if (!longHandled && (nowMs - startMs) >= kLongPressMs) {
            longHandled = true;
#if defined(MELODICA_WEB_CONFIG)
            LOG_INFO("Long press: starting config hotspot");
            if (gPortal) gPortal->startHotspot();
#else
            LOG_INFO("Long press (no web config on this profile)");
#endif
        }
    } else if (!pressed && down) {
        down = false;
        if (!longHandled && (nowMs - startMs) <= kShortPressMaxMs) {
            gSafety->triggerPanic(nowUs);
        }
    }
}

void loop() {
    const uint32_t nowUs = micros();

    // 1) Emergency stop first: nothing else in the loop may delay it.
    serviceEmergencyStop(nowUs);

    // 2) Pump the transport and drain decoded events into the core.
    gTransport->update();
    MidiEvent ev;
    while (gTransport->read(ev)) {
        if (gInstrument->handleEvent(ev)) {
            gSafety->noteMidiActivity(nowUs);
        }
    }

    // 3) Detect a link drop and force a safe stop immediately.
    bool connected = gTransport->connected();
    if (gWasConnected && !connected) {
        LOG_WARN("Transport disconnected: releasing notes and closing air");
        gInstrument->onTransportLost(nowUs);
    }
    gWasConnected = connected;

    // 4) BOOT button: short = panic, long = config hotspot.
    serviceConfigButton(nowUs);

    // 5) Advance schedulers, safety watchdogs, console and web portal.
    gInstrument->update(nowUs);
    gSafety->update(nowUs, connected);
    if (gConsole) gConsole->update();
#if defined(MELODICA_WEB_CONFIG)
    if (gPortal) gPortal->update();
#endif
}
