#ifndef CORE_IAIRCONTROLLER_H
#define CORE_IAIRCONTROLLER_H

#include "AirDemand.h"

namespace melodica {

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
};

}  // namespace melodica

#endif  // CORE_IAIRCONTROLLER_H
