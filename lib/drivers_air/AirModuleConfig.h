#ifndef DRIVERS_AIRMODULECONFIG_H
#define DRIVERS_AIRMODULECONFIG_H

#include <cstdint>

namespace melodica {

// Pin/hardware wiring for each concrete air module. Behavioural parameters
// (thresholds, ramps, inversion, precharge, min/max) live in AirConfig and are
// shared through AirRamp; this struct only carries per-actuator wiring.
struct AirModuleWiring {
    // Servo / PWM valve / blower / DAC output pin.
    int8_t primaryPin = -1;
    // H-bridge second pin (pump direction) / secondary control.
    int8_t secondaryPin = -1;
    // Digital enable/permission pin (external air, blower enable).
    int8_t enablePin = -1;
    // Pump/bellows direction: true = forward.
    bool forward = true;
    // LEDC channel + resolution for PWM actuators.
    uint8_t ledcChannel = 0;
    uint8_t ledcResolutionBits = 8;
    // Stepped-solenoid bank: up to 4 stages, ordered by increasing flow.
    int8_t stagePins[4] = {-1, -1, -1, -1};
    uint8_t stageCount = 0;
    // Stepper (bellows): step + dir pins and travel range in steps.
    int8_t stepPin = -1;
    int8_t dirPin = -1;
    uint16_t stepperMaxSteps = 2000;
};

}  // namespace melodica

#endif  // DRIVERS_AIRMODULECONFIG_H
