#ifndef CORE_PCAMAPPING_H
#define CORE_PCAMAPPING_H

#include <cstdint>

namespace melodica {

// A PCA9685 exposes 16 PWM channels (0..15). The previous firmware used 15,
// which silently dropped one channel per board and mis-routed every servo past
// the first 15. These helpers implement the corrected, canonical mapping:
//     driverIndex = servoIndex / CHANNELS_PER_PCA9685
//     channel     = servoIndex % CHANNELS_PER_PCA9685
constexpr uint8_t CHANNELS_PER_PCA9685 = 16;

struct PcaLocation {
    uint8_t driverIndex;
    uint8_t channel;
};

constexpr PcaLocation pcaLocationForServo(uint16_t servoIndex) {
    return PcaLocation{
        static_cast<uint8_t>(servoIndex / CHANNELS_PER_PCA9685),
        static_cast<uint8_t>(servoIndex % CHANNELS_PER_PCA9685)};
}

// Number of PCA9685 boards required to drive `servoCount` servos (ceil div).
constexpr uint8_t pcaBoardsRequired(uint16_t servoCount) {
    return static_cast<uint8_t>((servoCount + CHANNELS_PER_PCA9685 - 1) /
                                CHANNELS_PER_PCA9685);
}

}  // namespace melodica

#endif  // CORE_PCAMAPPING_H
