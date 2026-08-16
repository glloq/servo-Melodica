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
                     const KeyCalibration (&calib)[MAX_NOTES], uint16_t keyCount)
        : i2c_(i2c),
          cfg_(cfg),
          calib_(calib),
          keyCount_(keyCount > MAX_NOTES ? MAX_NOTES : keyCount) {
        for (uint8_t b = 0; b < MAX_PCA_BOARDS; ++b) {
            boards_[b] = Adafruit_PWMServoDriver(cfg.addresses[b]);
            boardOk_[b] = false;
        }
        for (uint16_t i = 0; i < keyCount_; ++i) {
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
        for (uint16_t i = 0; i < keyCount_; ++i) writePulse(i, currentPulse_[i]);
        return allHealthy_;
    }

    uint16_t keyCount() const override { return keyCount_; }

    void pressKey(uint16_t i) override {
        if (i < keyCount_) targetPulse_[i] = keyTargetPulseUs(calib_[i], true);
    }
    void releaseKey(uint16_t i) override {
        if (i < keyCount_) targetPulse_[i] = keyTargetPulseUs(calib_[i], false);
    }
    void releaseAll() override {
        for (uint16_t i = 0; i < keyCount_; ++i) {
            targetPulse_[i] = keyTargetPulseUs(calib_[i], false);
            currentPulse_[i] = targetPulse_[i];
            writePulse(i, currentPulse_[i]);
        }
    }

    void update(uint32_t nowUs) override {
        serviceHealthCheck(nowUs);
        if (cfg_.slewUsPerMs == 0) {
            // Instant move: only write when the target changed.
            for (uint16_t i = 0; i < keyCount_; ++i) {
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
        for (uint16_t i = 0; i < keyCount_; ++i) {
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
    // Runtime I2C supervision.
    //
    // begin() probing the bus once only proves the boards were there at power-up:
    // a board that browns out, loses its ground or has its cable pulled mid-set
    // would keep "healthy" forever while its 16 keys silently stop moving. So one
    // board is re-probed per interval (round-robin, a single address ACK — tens
    // of microseconds), which is cheap enough for the real-time loop and detects
    // the loss within boardCount * healthCheckIntervalMs.
    //
    // A board that comes back is re-initialised and re-commanded to its current
    // pulses, so recovering the cable recovers the instrument.
    void serviceHealthCheck(uint32_t nowUs) {
        if (cfg_.healthCheckIntervalMs == 0) return;
        uint8_t needed = cfg_.boardCount > MAX_PCA_BOARDS ? MAX_PCA_BOARDS : cfg_.boardCount;
        if (needed == 0) return;
        uint32_t intervalUs = static_cast<uint32_t>(cfg_.healthCheckIntervalMs) * 1000u;
        if (lastProbeUs_ != 0 && (nowUs - lastProbeUs_) < intervalUs) return;
        lastProbeUs_ = nowUs;

        uint8_t b = probeIndex_ % needed;
        probeIndex_ = static_cast<uint8_t>((probeIndex_ + 1) % needed);

        Wire.beginTransmission(cfg_.addresses[b]);
        bool answering = (Wire.endTransmission() == 0);
        if (answering && !boardOk_[b]) {
            // Board is back: re-init it and restore its keys.
            boards_[b].begin();
            boards_[b].setOscillatorFrequency(27000000);
            boards_[b].setPWMFreq(cfg_.pwmFrequencyHz);
            boardOk_[b] = true;
            for (uint16_t i = 0; i < keyCount_; ++i) {
                if (pcaLocationForServo(i).driverIndex == b) writePulse(i, currentPulse_[i]);
            }
        } else {
            boardOk_[b] = answering;
        }

        bool all = true;
        for (uint8_t i = 0; i < needed; ++i) all = all && boardOk_[i];
        allHealthy_ = all;
    }

    void writePulse(uint16_t i, uint16_t pulseUs) {
        PcaLocation loc = pcaLocationForServo(i);
        if (loc.driverIndex >= MAX_PCA_BOARDS || !boardOk_[loc.driverIndex]) return;
        uint16_t ticks = pulseUsToTicks(pulseUs, cfg_.pwmFrequencyHz);
        boards_[loc.driverIndex].setPWM(loc.channel, 0, ticks);
    }

    I2cConfig i2c_;
    KeyDriverConfig cfg_;
    const KeyCalibration (&calib_)[MAX_NOTES];
    Adafruit_PWMServoDriver boards_[MAX_PCA_BOARDS];
    bool boardOk_[MAX_PCA_BOARDS];
    bool allHealthy_ = false;
    uint16_t targetPulse_[MAX_NOTES];
    uint16_t currentPulse_[MAX_NOTES];
    uint16_t keyCount_;
    uint32_t lastUpdateUs_ = 0;
    uint32_t lastProbeUs_ = 0;
    uint8_t probeIndex_ = 0;
};

}  // namespace melodica

#endif  // DRIVERS_PCA9685KEYDRIVER_H
