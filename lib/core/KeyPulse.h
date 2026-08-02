#ifndef CORE_KEYPULSE_H
#define CORE_KEYPULSE_H

#include <cstdint>

#include "Config.h"

namespace melodica {

// Pure, host-testable servo pulse maths shared by the PCA9685 driver and the
// unit tests. Keeping it here guarantees the mechanical-limit clamping that the
// tests verify is exactly what the hardware driver executes.

inline uint16_t clampPulse(uint16_t pulseUs, uint16_t minUs, uint16_t maxUs) {
    if (pulseUs < minUs) return minUs;
    if (pulseUs > maxUs) return maxUs;
    return pulseUs;
}

// Target pulse (microseconds) for a key in the pressed/rest state, honouring the
// per-key inversion flag and clamped into the per-key mechanical limits. A bad
// calibration or an over-shooting ramp can therefore never command a servo past
// its safe travel.
inline uint16_t keyTargetPulseUs(const KeyCalibration& c, bool pressed) {
    bool effectivePressed = c.inverted ? !pressed : pressed;
    uint16_t target = effectivePressed ? c.pressedPulseUs : c.restPulseUs;
    return clampPulse(target, c.minPulseUs, c.maxPulseUs);
}

// Convert a pulse width in microseconds to a 12-bit PCA9685 "off" tick count at
// the given PWM frequency. Integer maths, no floats.
inline uint16_t pulseUsToTicks(uint16_t pulseUs, uint16_t pwmFreqHz) {
    uint32_t ticks = (static_cast<uint32_t>(pulseUs) * pwmFreqHz * 4096u) / 1000000u;
    if (ticks > 4095u) ticks = 4095u;
    return static_cast<uint16_t>(ticks);
}

}  // namespace melodica

#endif  // CORE_KEYPULSE_H
