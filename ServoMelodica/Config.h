#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <cstdint>

#include "AirDemand.h"

namespace melodica {

// ---------------------------------------------------------------------------
// Instrument dimensions
// ---------------------------------------------------------------------------
// Arrays are sized to MAX_NOTES (the hardware cap: 4 PCA9685 * 16 = 64 keys) so
// any melodica up to that size is supported without recompiling. The ACTUAL key
// count and the lowest MIDI note are runtime settings (InstrumentConfig, stored
// in NVS, editable from the web UI). NUMBER_OF_NOTES is the compiled default.
constexpr uint16_t MAX_NOTES = 64;
constexpr uint16_t NUMBER_OF_NOTES = 32;  // default key count
constexpr uint8_t FIRST_MIDI_NOTE = 65;   // default lowest key = F4 (see README)

struct InstrumentConfig {
    uint8_t noteCount = NUMBER_OF_NOTES;    // number of playable keys (<= MAX_NOTES)
    uint8_t firstMidiNote = FIRST_MIDI_NOTE;  // MIDI number of the lowest key
};

// ---------------------------------------------------------------------------
// Per-key mechanical calibration
// ---------------------------------------------------------------------------
// Pulse widths are expressed in microseconds (typical hobby servo 500..2500us).
// restPulseUs   : key released
// pressedPulseUs: key fully pressed
// minPulseUs/maxPulseUs : absolute mechanical limits; the driver clamps every
//                 commanded pulse into [min,max] so a bad calibration or ramp
//                 can never drive a servo past its safe travel.
// inverted      : swap the meaning of rest/pressed for reversed linkages.
struct KeyCalibration {
    uint16_t restPulseUs = 1500;
    uint16_t pressedPulseUs = 1900;
    uint16_t minPulseUs = 500;
    uint16_t maxPulseUs = 2500;
    bool inverted = false;
};

// ---------------------------------------------------------------------------
// I2C / PCA9685 configuration
// ---------------------------------------------------------------------------
constexpr uint8_t MAX_PCA_BOARDS = 4;  // 4 * 16 = 64 channels available

struct I2cConfig {
    int8_t sdaPin = 21;
    int8_t sclPin = 22;
    uint32_t clockHz = 400000;
};

struct KeyDriverConfig {
    uint8_t boardCount = 2;                                    // number of PCA9685s
    uint8_t addresses[MAX_PCA_BOARDS] = {0x40, 0x41, 0x42, 0x43};
    int8_t oePin = -1;             // shared OE pin (active-low), -1 = not wired
    uint16_t pwmFrequencyHz = 50;  // servo refresh rate
    // Optional slew-rate limit in microseconds-of-pulse per millisecond.
    // 0 disables ramping (instant move). Provides the "vitesse/rampe" feature.
    uint16_t slewUsPerMs = 0;
    // Runtime I2C supervision: one board is re-probed every healthCheckIntervalMs
    // (round-robin, one address per cycle) so a board that dies AFTER begin() is
    // detected. 0 disables the runtime check (begin()-time probe only).
    uint16_t healthCheckIntervalMs = 500;
};

// ---------------------------------------------------------------------------
// Air system configuration
// ---------------------------------------------------------------------------
// Which physical air system is used. Selectable at runtime (stored in NVS, set
// from the web UI / serial console) so a single binary can drive any of them;
// lean single-module builds still exist via the -DAIR_* compile flags.
enum class AirSystemType : uint8_t {
    ServoValve = 0,       // proportional valve on a hobby servo
    Solenoid,             // on/off solenoid
    SteppedSolenoid,      // several solenoids in flow steps
    BlowerPwm,            // DC blower/fan via PWM
    PumpHBridge,          // DC pump + H-bridge (direction configurable)
    PwmValve,             // proportional valve via PWM
    PwmValveDac,          // proportional valve via DAC (ESP32/S2 only)
    StepperBellows,       // bellows driven by a stepper
    ExternalAir,          // permanent external air + digital enable
    Simulation,           // no actuator
    Composite             // 4-stage: source -> main valve -> flow -> (sensor)
};

// --- Composable air-system stages (used when type == Composite) -------------
// A composite air system is built from four independent, separately-configurable
// stages, e.g. a solenoid MAIN VALVE + a servo FLOW ("servoflow") fed by a
// blower/pump/reservoir SOURCE, optionally regulated by a pressure SENSOR.
enum class MainValveKind : uint8_t { None, Solenoid };
enum class FlowKind : uint8_t { None, Servo, Pwm, Dac };
enum class SourceKind : uint8_t { External, Blower, Pump, Reservoir };

struct CompositeAirConfig {
    MainValveKind mainValve = MainValveKind::Solenoid;
    FlowKind flow = FlowKind::Servo;
    SourceKind source = SourceKind::Blower;

    // Main valve
    int8_t mainValvePin = 26;
    bool mainValveActiveHigh = true;

    // Flow actuator ("servoflow" / PWM / DAC)
    int8_t flowPin = 13;
    uint8_t flowLedcChannel = 4;

    // Source (blower/pump)
    int8_t sourcePwmPin = 32;
    int8_t sourceDirPin = 33;      // pump direction
    int8_t sourceEnablePin = 25;   // external-air enable / source on-off
    uint8_t sourceLedcChannel = 5;
    bool sourceForward = true;

    // Optional pressure sensor + reservoir regulation
    bool sensorEnabled = false;
    int8_t sensorPin = 34;         // ADC-capable GPIO
    uint16_t sensorMinRaw = 0;     // raw ADC at 0 pressure
    uint16_t sensorMaxRaw = 4095;  // raw ADC at full pressure
    uint8_t targetPressure = 200;  // 0..255 reservoir set-point
    uint8_t pressureHysteresis = 20;  // bang-bang band around the set-point
};

// Per-actuator wiring, persisted in config so pins are editable from the UI.
// Only the fields relevant to the selected AirSystemType are used.
struct AirModuleWiring {
    int8_t primaryPin = 13;    // servo / PWM / blower / DAC output
    int8_t secondaryPin = 14;  // H-bridge direction / secondary control
    int8_t enablePin = 27;     // digital enable/permission (external, blower)
    bool forward = true;       // pump/bellows direction
    uint8_t ledcChannel = 4;   // LEDC channel for PWM actuators
    uint8_t ledcResolutionBits = 8;
    int8_t stagePins[4] = {32, 33, -1, -1};  // stepped-solenoid bank
    uint8_t stageCount = 2;
    int8_t stepPin = 18;       // stepper (bellows)
    int8_t dirPin = 19;
    uint16_t stepperMaxSteps = 2000;
};

struct AirConfig {
    AirSystemType type = AirSystemType::ServoValve;
    AirModuleWiring wiring;         // used by the single-actuator module types
    CompositeAirConfig composite;   // used when type == Composite
    AirPolicy policy = AirPolicy::MaximumVelocity;
    bool inverted = false;
    uint32_t pwmFrequencyHz = 20000;  // for blower/pump/PWM-valve modules
    uint8_t startThreshold = 8;       // demand below this counts as "off"
    uint8_t minOutput = 20;           // actuator floor once started (0..255)
    uint8_t maxOutput = 255;          // actuator ceiling (0..255)
    uint16_t attackRampMs = 40;       // ramp-up time to target
    uint16_t releaseRampMs = 120;     // ramp-down time to target
    uint8_t precharge = 0;            // initial kick output on first demand
    // Non-blocking air/key synchronisation window (see KeyAirScheduler):
    uint16_t airPrechargeMs = 30;     // open air this long before pressing a key
    uint16_t keyPressDelayMs = 0;     // wait after air-open before key press
    uint16_t keyReleaseDelayMs = 0;   // wait after note-off before key release
    uint16_t airReleaseDelayMs = 40;  // keep air this long after key release
};

// ---------------------------------------------------------------------------
// MIDI filtering / mapping
// ---------------------------------------------------------------------------
enum class ChannelMode : uint8_t { Single, List, Omni };

struct MidiFilterConfig {
    ChannelMode channelMode = ChannelMode::Omni;
    uint16_t channelMask = 0xFFFF;  // bit N set => channel N accepted (List mode)
    uint8_t singleChannel = 0;      // used in Single mode
    int8_t transpose = 0;           // semitone offset applied before mapping
    uint8_t lowNote = 0;            // reject notes below this (post-transpose)
    uint8_t highNote = 127;         // reject notes above this (post-transpose)
};

// ---------------------------------------------------------------------------
// Network (WiFi / RTP-MIDI) configuration
// ---------------------------------------------------------------------------
// Credentials are NEVER compiled with real values and NEVER committed. They are
// loaded from NVS at runtime; the defaults here are empty placeholders.
constexpr uint8_t WIFI_SSID_MAX = 33;
constexpr uint8_t WIFI_PASS_MAX = 65;

struct NetworkConfig {
    char ssid[WIFI_SSID_MAX] = {0};
    char password[WIFI_PASS_MAX] = {0};
    char sessionName[24] = "Servo Melodica";
    uint16_t reconnectBaseMs = 500;   // backoff base
    uint16_t reconnectMaxMs = 30000;  // backoff ceiling
};

// ---------------------------------------------------------------------------
// Safety limits
// ---------------------------------------------------------------------------
struct SafetyConfig {
    uint32_t commTimeoutMs = 5000;      // no MIDI + no connection => safe stop
    uint32_t maxKeyHoldMs = 30000;      // stuck-key watchdog
    uint32_t maxPumpRunMs = 120000;     // pump/blower duty watchdog (0 = disabled)
    bool disableOutputsOnIdle = true;   // drop OE after air release when idle
    uint32_t idleDisableDelayMs = 2000; // wait this long idle before dropping OE
    // Byte-stream links (DIN / serial) are "up" as long as the UART is open: a
    // silent MIDI cable is NOT a disconnected one. Link supervision is only armed
    // once the sender uses Active Sensing (0xFE), which is exactly the promise
    // "I will keep talking"; this is the timeout applied from then on.
    uint16_t activeSensingTimeoutMs = 300;
    // Dedicated hardware emergency stop, independent of the BOOT button.
    // -1 = none. A normally-CLOSED button to GND with the internal pull-up is the
    // recommended wiring: opening the loop (or a cut wire) reads HIGH => set
    // panicActiveHigh = true so a broken wire also triggers the stop.
    int8_t panicPin = -1;
    bool panicActiveHigh = true;
    uint16_t panicDebounceMs = 5;
};

// ---------------------------------------------------------------------------
// Persisted, versioned root config
// ---------------------------------------------------------------------------
// v3: safety (active sensing, panic input), key-driver health check.
constexpr uint16_t CONFIG_SCHEMA_VERSION = 3;

struct BoardConfig {
    uint16_t schemaVersion = CONFIG_SCHEMA_VERSION;
    InstrumentConfig instrument;
    I2cConfig i2c;
    KeyDriverConfig keyDriver;
    AirConfig air;
    MidiFilterConfig midi;
    NetworkConfig network;
    SafetyConfig safety;
    KeyCalibration keys[MAX_NOTES];  // sized to the cap; instrument.noteCount used
    uint32_t crc32 = 0;  // integrity check, computed over everything above
};

}  // namespace melodica

#endif  // CORE_CONFIG_H
