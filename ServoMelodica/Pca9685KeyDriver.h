#ifndef DRIVERS_PCA9685KEYDRIVER_H
#define DRIVERS_PCA9685KEYDRIVER_H

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

#include "Config.h"
#include "IKeyDriver.h"
#include "KeyPulse.h"
#include "PcaMapping.h"

namespace melodica {

// IKeyDriver backed by 1..MAX_PCA_BOARDS PCA9685 boards.
//
// Corrects the historical defects:
//  * 16 channels per board (was 15), with driverIndex = i/16, channel = i%16.
//  * Configurable board count and I2C address list, verified at begin().
//  * Per-key rest/pressed pulses, individual min/max mechanical clamps and
//    inversion, driven by the shared KeyPulse maths.
//  * OE handled as active-low OUTPUT-DISABLE (not a servo power cut): HIGH on OE
//    tri-states the PWM outputs, LOW enables them.
//  * Optional non-blocking slew limiting instead of blocking delay() moves.
//  * Per-board fault flag; healthy() is false if any board failed to answer.
class Pca9685KeyDriver : public IKeyDriver {
public:
    Pca9685KeyDriver(const I2cConfig& i2c, const KeyDriverConfig& cfg,
                     const KeyCalibration (&calib)[NUMBER_OF_NOTES])
        : i2c_(i2c), cfg_(cfg), calib_(calib) {
        for (uint8_t b = 0; b < MAX_PCA_BOARDS; ++b) {
            boards_[b] = Adafruit_PWMServoDriver(cfg.addresses[b]);
            boardOk_[b] = false;
        }
        for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
            targetPulse_[i] = keyTargetPulseUs(calib_[i], false);
            currentPulse_[i] = targetPulse_[i];
        }
    }

    bool begin() override {
        if (cfg_.oePin >= 0) {
            pinMode(cfg_.oePin, OUTPUT);
            digitalWrite(cfg_.oePin, LOW);  // active-low: LOW = outputs enabled
        }
        Wire.begin(i2c_.sdaPin, i2c_.sclPin);
        Wire.setClock(i2c_.clockHz);

        uint8_t needed = cfg_.boardCount;
        if (needed > MAX_PCA_BOARDS) needed = MAX_PCA_BOARDS;

        allHealthy_ = true;
        for (uint8_t b = 0; b < needed; ++b) {
            Wire.beginTransmission(cfg_.addresses[b]);
            if (Wire.endTransmission() != 0) {
                boardOk_[b] = false;
                allHealthy_ = false;
                continue;  // absent board: don't block, just mark unhealthy
            }
            boards_[b].begin();
            boards_[b].setOscillatorFrequency(27000000);
            boards_[b].setPWMFreq(cfg_.pwmFrequencyHz);
            boardOk_[b] = true;
        }
        // Command all keys to their rest position.
        for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) writePulse(i, currentPulse_[i]);
        return allHealthy_;
    }

    uint16_t keyCount() const override { return NUMBER_OF_NOTES; }

    void pressKey(uint16_t i) override {
        if (i < NUMBER_OF_NOTES) targetPulse_[i] = keyTargetPulseUs(calib_[i], true);
    }
    void releaseKey(uint16_t i) override {
        if (i < NUMBER_OF_NOTES) targetPulse_[i] = keyTargetPulseUs(calib_[i], false);
    }
    void releaseAll() override {
        for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
            targetPulse_[i] = keyTargetPulseUs(calib_[i], false);
            currentPulse_[i] = targetPulse_[i];
            writePulse(i, currentPulse_[i]);
        }
    }

    void update(uint32_t nowUs) override {
        if (cfg_.slewUsPerMs == 0) {
            // Instant move: only write when the target changed.
            for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
                if (currentPulse_[i] != targetPulse_[i]) {
                    currentPulse_[i] = targetPulse_[i];
                    writePulse(i, currentPulse_[i]);
                }
            }
            return;
        }
        // Non-blocking slew toward each target.
        uint32_t dtMs = lastUpdateUs_ == 0 ? 0 : (nowUs - lastUpdateUs_) / 1000u;
        lastUpdateUs_ = nowUs;
        if (dtMs == 0) return;
        uint16_t step = static_cast<uint16_t>(cfg_.slewUsPerMs * dtMs);
        for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
            if (currentPulse_[i] == targetPulse_[i]) continue;
            if (currentPulse_[i] < targetPulse_[i]) {
                currentPulse_[i] = (targetPulse_[i] - currentPulse_[i] <= step)
                                       ? targetPulse_[i]
                                       : currentPulse_[i] + step;
            } else {
                currentPulse_[i] = (currentPulse_[i] - targetPulse_[i] <= step)
                                       ? targetPulse_[i]
                                       : currentPulse_[i] - step;
            }
            writePulse(i, currentPulse_[i]);
        }
    }

    void setOutputsEnabled(bool enabled) override {
        if (cfg_.oePin < 0) return;
        // Active-low OE: LOW enables outputs, HIGH disables (tri-state) them.
        // This does NOT remove power from the servos; it only stops commanding
        // the PWM outputs to reduce chatter/noise when idle.
        digitalWrite(cfg_.oePin, enabled ? LOW : HIGH);
    }

    bool healthy() const override { return allHealthy_; }

private:
    void writePulse(uint16_t i, uint16_t pulseUs) {
        PcaLocation loc = pcaLocationForServo(i);
        if (loc.driverIndex >= MAX_PCA_BOARDS || !boardOk_[loc.driverIndex]) return;
        uint16_t ticks = pulseUsToTicks(pulseUs, cfg_.pwmFrequencyHz);
        boards_[loc.driverIndex].setPWM(loc.channel, 0, ticks);
    }

    I2cConfig i2c_;
    KeyDriverConfig cfg_;
    const KeyCalibration (&calib_)[NUMBER_OF_NOTES];
    Adafruit_PWMServoDriver boards_[MAX_PCA_BOARDS];
    bool boardOk_[MAX_PCA_BOARDS];
    bool allHealthy_ = false;
    uint16_t targetPulse_[NUMBER_OF_NOTES];
    uint16_t currentPulse_[NUMBER_OF_NOTES];
    uint32_t lastUpdateUs_ = 0;
};

}  // namespace melodica

#endif  // DRIVERS_PCA9685KEYDRIVER_H
