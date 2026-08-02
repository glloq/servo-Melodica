#ifndef CORE_SAFETYMANAGER_H
#define CORE_SAFETYMANAGER_H

#include <cstdint>

#include "Config.h"
#include "IAirController.h"
#include "IKeyDriver.h"
#include "InstrumentController.h"

namespace melodica {

// Machine-readable fault codes, kept consultable after a fault so the main loop
// or a serial console can report why the instrument shut down.
enum class SafetyFault : uint8_t {
    None = 0,
    CommTimeout,        // no MIDI and no connection for commTimeoutMs
    TransportLost,      // link dropped while notes were active
    I2cFault,           // a PCA board stopped answering
    AirNotInitialised,  // air actuator never came up
    Panic,              // explicit panic / SystemReset
    KeyStuck,           // a key was held longer than maxKeyHoldMs
    PumpOverrun         // air actuator ran longer than maxPumpRunMs
};

// Supervises the instrument and forces a safe state on any critical fault:
// release all keys, close air, disable PWM outputs, latch an error code, and
// attempt recovery once conditions are safe again.
class SafetyManager {
public:
    SafetyManager(const SafetyConfig& config, InstrumentController& instrument,
                  IKeyDriver& keys, IAirController& air);

    // Record that a valid MIDI event was just processed (feeds the comm watchdog).
    void noteMidiActivity(uint32_t nowUs);

    // Poll the watchdogs. `connected` is the active transport's link state.
    void update(uint32_t nowUs, bool connected);

    // Explicit local panic (e.g. hardware button).
    void triggerPanic(uint32_t nowUs);

    SafetyFault fault() const { return fault_; }
    const char* faultText() const;
    bool inFault() const { return fault_ != SafetyFault::None; }

    // Clear a latched fault once the operator/host has acknowledged it.
    void clearFault();

private:
    void enterFault(SafetyFault fault, uint32_t nowUs);

    const SafetyConfig& config_;
    InstrumentController& instrument_;
    IKeyDriver& keys_;
    IAirController& air_;

    SafetyFault fault_ = SafetyFault::None;
    uint32_t faultAtUs_ = 0;
    uint32_t lastActivityUs_ = 0;
    bool activitySeen_ = false;  // distinct from timestamp==0
    bool everConnected_ = false;
    bool outputsDisabled_ = false;
    uint32_t idleSinceUs_ = 0;
    bool idleTracking_ = false;
};

}  // namespace melodica

#endif  // CORE_SAFETYMANAGER_H
