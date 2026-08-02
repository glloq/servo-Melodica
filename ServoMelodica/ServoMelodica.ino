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
//    A SHORT press is a local panic.
// ===========================================================================
#include <Arduino.h>

#include "UserConfig.h"  // compile-time defaults (transport) — edit in Arduino IDE

#include "AirControllerFactory.h"
#include "Config.h"
#include "Defaults.h"
#include "InstrumentController.h"
#include "Log.h"
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
constexpr int8_t kPanicButtonPin = 0;  // BOOT button, active-low
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
static bool gWasConnected = false;
#if defined(MELODICA_WEB_CONFIG)
static WebConfigPortal* gPortal = nullptr;
#endif

void setup() {
    Serial.begin(115200);
    delay(200);
    LOG_INFO("Servo Melodica unified firmware starting");

    gConfig = ConfigStore::load();

    // --- Key driver (PCA9685 x N) ---
    static Pca9685KeyDriver keys(gConfig.i2c, gConfig.keyDriver, gConfig.keys);
    gKeys = &keys;

    // --- Air module: selected at RUNTIME from gConfig.air.type ---
    static AirControllerStorage airStorage;
    gAir = createAirController(gConfig, airStorage);

    static InstrumentController instrument(gConfig, keys, *gAir);
    gInstrument = &instrument;

    static SafetyManager safety(gConfig.safety, instrument, keys, *gAir);
    gSafety = &safety;

    // --- Transport (exactly one) ---
#if defined(TRANSPORT_BLE)
    static BleMidiTransport<decltype(MIDI), decltype(BLEMIDI)> transport(MIDI, BLEMIDI);
#elif defined(TRANSPORT_WIFI_RTP)
    static RtpMidiTransport<decltype(MIDI), decltype(AppleMIDI)> transport(
        MIDI, AppleMIDI, gConfig.network.ssid, gConfig.network.password,
        gConfig.network.reconnectBaseMs, gConfig.network.reconnectMaxMs);
#elif defined(TRANSPORT_DIN)
    static DinMidiTransport transport(Serial2, 16, 17);
#elif defined(TRANSPORT_USB)
    static UsbMidiTransport<decltype(MIDI)> transport(MIDI);
#elif defined(TRANSPORT_SERIAL)
    static SerialMidiTransport transport(Serial2, 115200, 16, 17);
#endif
    gTransport = &transport;

    pinMode(kPanicButtonPin, INPUT_PULLUP);

    if (!instrument.begin()) {
        LOG_ERROR("Instrument init reported a hardware problem (continuing safely)");
    }
    transport.begin();
    LOG_INFO("Transport: %s", transport.name());

#if defined(MELODICA_WEB_CONFIG)
    static WebConfigPortal portal(gConfig);
    gPortal = &portal;
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

// Distinguish a short BOOT press (panic) from a long press (start hotspot).
static void serviceButton(uint32_t nowUs) {
    static bool down = false;
    static uint32_t startMs = 0;
    static bool longHandled = false;
    const uint32_t nowMs = millis();
    const bool pressed = digitalRead(kPanicButtonPin) == LOW;

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

    // 1) Pump the transport and drain decoded events into the core.
    gTransport->update();
    MidiEvent ev;
    while (gTransport->read(ev)) {
        if (gInstrument->handleEvent(ev)) {
            gSafety->noteMidiActivity(nowUs);
        }
    }

    // 2) Detect a link drop and force a safe stop immediately.
    bool connected = gTransport->connected();
    if (gWasConnected && !connected) {
        LOG_WARN("Transport disconnected: releasing notes and closing air");
        gInstrument->onTransportLost(nowUs);
    }
    gWasConnected = connected;

    // 3) BOOT button: short = panic, long = config hotspot.
    serviceButton(nowUs);

    // 4) Advance schedulers, safety watchdogs, console and web portal.
    gInstrument->update(nowUs);
    gSafety->update(nowUs, connected);
    if (gConsole) gConsole->update();
#if defined(MELODICA_WEB_CONFIG)
    if (gPortal) gPortal->update();
#endif
}
