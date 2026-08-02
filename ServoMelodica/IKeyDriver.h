#ifndef CORE_IKEYDRIVER_H
#define CORE_IKEYDRIVER_H

#include <cstdint>

namespace melodica {

// Abstraction over whatever hardware presses the physical keys. The reference
// implementation is Pca9685KeyDriver, but a native mock driver implements the
// same interface so the whole core can be tested on a PC.
class IKeyDriver {
public:
    virtual ~IKeyDriver() = default;

    // Bring up the hardware (I2C probing, etc.). Non-blocking; returns false if
    // no board could be initialised.
    virtual bool begin() = 0;

    // Number of keys this driver can actuate.
    virtual uint16_t keyCount() const = 0;

    // Press / release a single key by index (0..keyCount()-1).
    virtual void pressKey(uint16_t keyIndex) = 0;
    virtual void releaseKey(uint16_t keyIndex) = 0;

    // Release every key immediately (panic / safety path).
    virtual void releaseAll() = 0;

    // Advance any per-key motion ramps. `nowUs` is micros(); must not block.
    virtual void update(uint32_t nowUs) = 0;

    // Enable/disable PCA outputs via the active-low OE pin. Disabling floats the
    // PWM outputs (servos stop being commanded) but does NOT cut servo power.
    virtual void setOutputsEnabled(bool enabled) = 0;

    // True if every configured board answered on I2C at begin().
    virtual bool healthy() const = 0;
};

}  // namespace melodica

#endif  // CORE_IKEYDRIVER_H
