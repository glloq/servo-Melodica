// ===========================================================================
//  Servo Melodica — unified firmware composition root
// ===========================================================================
//  A single source tree drives every ESP32 profile. Build flags (set per
//  PlatformIO environment) select exactly one MIDI transport and one air module;
//  the transport-agnostic core (lib/core) is shared by all of them with no
//  duplication. See platformio.ini for the profiles and README for details.
//
//  Transport flags (choose one):  TRANSPORT_BLE | TRANSPORT_WIFI_RTP |
//                                 TRANSPORT_DIN | TRANSPORT_USB | TRANSPORT_SERIAL
//  Air flags (choose one):        AIR_SERVO_VALVE | AIR_SOLENOID |
//                                 AIR_STEPPED_SOLENOID | AIR_BLOWER_PWM |
//                                 AIR_PUMP_HBRIDGE | AIR_PWM_VALVE |
//                                 AIR_STEPPER_BELLOWS | AIR_EXTERNAL | AIR_SIMULATION
// ===========================================================================
#include <Arduino.h>

#include "Config.h"
#include "Defaults.h"
#include "InstrumentController.h"
#include "Log.h"
#include "Pca9685KeyDriver.h"
#include "SafetyManager.h"
#include "CalibrationConsole.h"
#include "ConfigStore.h"

// --- Air module selection --------------------------------------------------
#include "AirModuleConfig.h"        // AirModuleWiring (needed by all profiles)
#include "SimulationAirController.h"
#if !defined(AIR_SIMULATION)
#include "HardwareAirControllers.h"
#endif

// --- Transport selection ---------------------------------------------------
#if defined(TRANSPORT_BLE)
#include <BLEMIDI_Transport.h>
// Define MELODICA_BLE_NIMBLE (and add the NimBLE-Arduino lib) to use the lighter
// NimBLE stack instead of Bluedroid; both expose the same BLEMIDI_CREATE_INSTANCE.
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

// Hardware pin map for air modules (edit or override per board as needed).
namespace {
constexpr int8_t kAirServoPin = 13;
constexpr int8_t kAirPwmPin = 13;
constexpr int8_t kAirDirPin = 14;
constexpr int8_t kAirEnablePin = 27;
constexpr int8_t kPanicButtonPin = 0;  // BOOT button, active-low
constexpr bool kEnableCalibrationConsole = true;

AirModuleWiring makeAirWiring() {
    AirModuleWiring w;
    w.primaryPin = kAirPwmPin;
    w.secondaryPin = kAirDirPin;
    w.enablePin = kAirEnablePin;
    w.ledcChannel = 4;
    w.ledcResolutionBits = 8;
    w.stagePins[0] = 32; w.stagePins[1] = 33; w.stageCount = 2;
    w.stepPin = 18; w.dirPin = 19; w.stepperMaxSteps = 2000;
    (void)kAirServoPin;
    return w;
}
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

void setup() {
    Serial.begin(115200);
    delay(200);
    LOG_INFO("Servo Melodica unified firmware starting");

    // Load persisted configuration (calibration, credentials, air params...).
    gConfig = ConfigStore::load();

    // --- Key driver (PCA9685 x N) ---
    static Pca9685KeyDriver keys(gConfig.i2c, gConfig.keyDriver, gConfig.keys);
    gKeys = &keys;

    // --- Air module (exactly one, chosen at build time) ---
    static AirModuleWiring wiring = makeAirWiring();
#if defined(AIR_SIMULATION)
    static SimulationAirController air(gConfig.air);
#elif defined(AIR_SERVO_VALVE)
    static ServoValveAirController air(gConfig.air, wiring);
#elif defined(AIR_SOLENOID)
    static SolenoidAirController air(gConfig.air, wiring);
#elif defined(AIR_STEPPED_SOLENOID)
    static SteppedSolenoidAirController air(gConfig.air, wiring);
#elif defined(AIR_BLOWER_PWM)
    static BlowerPwmAirController air(gConfig.air, wiring);
#elif defined(AIR_PUMP_HBRIDGE)
    static PumpHBridgeAirController air(gConfig.air, wiring);
#elif defined(AIR_PWM_VALVE)
    static PwmValveAirController air(gConfig.air, wiring, /*useDac=*/false);
#elif defined(AIR_STEPPER_BELLOWS)
    static StepperBellowsAirController air(gConfig.air, wiring);
#elif defined(AIR_EXTERNAL)
    static ExternalAirController air(gConfig.air, wiring);
#else
#error "Select an air module (AIR_* build flag)"
#endif
    gAir = &air;

    // --- Instrument core ---
    static InstrumentController instrument(gConfig, keys, air);
    gInstrument = &instrument;

    // --- Safety supervisor ---
    static SafetyManager safety(gConfig.safety, instrument, keys, air);
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
    // Dedicated UART (Serial2, RX=16/TX=17) so MIDI never collides with the
    // log/console on Serial.
    static SerialMidiTransport transport(Serial2, 115200, 16, 17);
#endif
    gTransport = &transport;

    pinMode(kPanicButtonPin, INPUT_PULLUP);

    if (!instrument.begin()) {
        LOG_ERROR("Instrument init reported a hardware problem (continuing safely)");
    }
    transport.begin();
    LOG_INFO("Transport: %s", transport.name());

    if (kEnableCalibrationConsole) {
        static CalibrationConsole console(gConfig, keys, Serial);
        gConsole = &console;
        console.begin();
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

    // 3) Local panic button (active-low).
    if (digitalRead(kPanicButtonPin) == LOW) {
        gSafety->triggerPanic(nowUs);
    }

    // 4) Advance schedulers, safety watchdogs and the optional console.
    gInstrument->update(nowUs);
    gSafety->update(nowUs, connected);
    if (gConsole) gConsole->update();
}
