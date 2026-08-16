#ifndef CORE_IAIRCONTROLLER_H
#define CORE_IAIRCONTROLLER_H

#include <cstdint>

#include "AirDemand.h"

namespace melodica {

// Why an air module refuses to run (or ran badly). Reported to SafetyManager,
// the serial console and the web status page.
enum class AirFault : uint8_t {
    None = 0,
    NotInitialised,  // begin() failed / never ran
    InvalidPin,      // a required GPIO is unusable (-1, input-only, no DAC, ...)
    InvalidSensor,   // pressure sensor missing or mis-calibrated (max <= min)
};

inline const char* airFaultText(AirFault f) {
    switch (f) {
        case AirFault::None: return "OK";
        case AirFault::NotInitialised: return "AIR_NOT_INITIALISED";
        case AirFault::InvalidPin: return "AIR_INVALID_PIN";
        case AirFault::InvalidSensor: return "AIR_INVALID_SENSOR";
    }
    return "UNKNOWN";
}

// Common interface for every air-supply module (proportional valve servo,
// on/off solenoid, blower, pump, stepper bellows, external air, simulation...).
// The core computes an AirDemand and hands it to whichever module was selected
// at build time; the module owns all actuator-specific behaviour.
class IAirController {
public:
    virtual ~IAirController() = default;

    // Bring up the actuator. Non-blocking; returns false on hard failure.
    virtual bool begin() = 0;

    // Update the target demand. Cheap; actual ramping happens in update().
    virtual void setDemand(const AirDemand& demand) = 0;

    // Advance ramps / PWM. `nowUs` is micros(); must not block.
    virtual void update(uint32_t nowUs) = 0;

    // Graceful close (release ramp applies).
    virtual void stop() = 0;

    // Immediate hard close, bypassing ramps (safety path).
    virtual void emergencyStop() = 0;

    // --- Supervision, consumed by SafetyManager -----------------------------
    // The base implementations describe a module with nothing to supervise (the
    // simulation module); every real actuator overrides them.

    // True while the module is actually moving air (valve open, pump energised).
    virtual bool isRunning() const { return false; }

    // Length of the CURRENT uninterrupted run in ms; 0 when stopped. This is
    // what SafetyConfig::maxPumpRunMs is compared against.
    virtual uint32_t runtimeMs() const { return 0; }

    // False when the module cannot be trusted to run (bad pins, missing sensor,
    // failed bring-up). SafetyManager latches AirNotInitialised on this.
    virtual bool healthy() const { return true; }

    // Machine-readable reason behind healthy() == false.
    virtual AirFault fault() const { return AirFault::None; }
};

}  // namespace melodica

#endif  // CORE_IAIRCONTROLLER_H
