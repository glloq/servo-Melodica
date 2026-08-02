#ifndef DRIVERS_HARDWAREAIRCONTROLLERS_H
#define DRIVERS_HARDWAREAIRCONTROLLERS_H

// Concrete air modules that drive real ESP32 hardware. Each one delegates the
// demand->level shaping to AirRamp (thresholds, ramps, precharge, inversion) and
// only implements how a level of 0..255 is applied to its actuator. Guarded by
// ARDUINO so the host test build never pulls in ESP32 headers.
#if defined(ARDUINO)

#include <Arduino.h>
#include <ESP32Servo.h>

#include "AirModuleConfig.h"
#include "AirRamp.h"
#include "IAirController.h"
#include "Log.h"

namespace melodica {

// LEDC PWM API compatibility shim.
//
// Arduino-ESP32 core 3.x removed the channel-based LEDC API (ledcSetup /
// ledcAttachPin / ledcWrite(channel,...)) and replaced it with a pin-based API
// (ledcAttachChannel / ledcWrite(pin,...)). These helpers compile on both cores
// so every air module below is source-compatible with core 2.x and 3.x.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
inline void ledcSetupPin(uint8_t pin, uint8_t ch, uint32_t freq, uint8_t resBits) {
    ledcAttachChannel(pin, freq, resBits, ch);
}
inline void ledcWritePin(uint8_t pin, uint8_t /*ch*/, uint32_t duty) {
    ledcWrite(pin, duty);
}
#else
inline void ledcSetupPin(uint8_t pin, uint8_t ch, uint32_t freq, uint8_t resBits) {
    ledcSetup(ch, freq, resBits);
    ledcAttachPin(pin, ch);
}
inline void ledcWritePin(uint8_t /*pin*/, uint8_t ch, uint32_t duty) {
    ledcWrite(ch, duty);
}
#endif

// 1) Proportional air valve driven by a hobby servo.
class ServoValveAirController : public IAirController {
public:
    ServoValveAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : ramp_(cfg), wiring_(w) {}
    bool begin() override {
        servo_.setPeriodHertz(50);
        servo_.attach(wiring_.primaryPin, 500, 2500);
        servo_.write(0);
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override { apply(ramp_.update(nowUs)); }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { apply(ramp_.forceClosed()); }
private:
    void apply(uint8_t level) { servo_.write(map(level, 0, 255, 0, 180)); }
    AirRamp ramp_;
    AirModuleWiring wiring_;
    Servo servo_;
};

// 2) On/off solenoid valve.
class SolenoidAirController : public IAirController {
public:
    SolenoidAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : ramp_(cfg), wiring_(w) {}
    bool begin() override {
        pinMode(wiring_.primaryPin, OUTPUT);
        digitalWrite(wiring_.primaryPin, LOW);
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override { apply(ramp_.update(nowUs)); }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { apply(ramp_.forceClosed()); }
private:
    void apply(uint8_t level) {
        digitalWrite(wiring_.primaryPin, level > 0 ? HIGH : LOW);
    }
    AirRamp ramp_;
    AirModuleWiring wiring_;
};

// 3) Several solenoids opened in flow steps.
class SteppedSolenoidAirController : public IAirController {
public:
    SteppedSolenoidAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : ramp_(cfg), wiring_(w) {}
    bool begin() override {
        for (uint8_t i = 0; i < wiring_.stageCount; ++i) {
            pinMode(wiring_.stagePins[i], OUTPUT);
            digitalWrite(wiring_.stagePins[i], LOW);
        }
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override { apply(ramp_.update(nowUs)); }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { apply(ramp_.forceClosed()); }
private:
    void apply(uint8_t level) {
        // Number of stages proportional to level (equal-width bands).
        uint8_t open = wiring_.stageCount == 0
                           ? 0
                           : static_cast<uint8_t>((level * wiring_.stageCount + 254) / 255);
        for (uint8_t i = 0; i < wiring_.stageCount; ++i) {
            digitalWrite(wiring_.stagePins[i], i < open ? HIGH : LOW);
        }
    }
    AirRamp ramp_;
    AirModuleWiring wiring_;
};

// 4) DC blower / fan via LEDC PWM.
class BlowerPwmAirController : public IAirController {
public:
    BlowerPwmAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : ramp_(cfg), wiring_(w), pwmFreq_(cfg.pwmFrequencyHz) {}
    bool begin() override {
        ledcSetupPin(wiring_.primaryPin, wiring_.ledcChannel, pwmFreq_, wiring_.ledcResolutionBits);
        if (wiring_.enablePin >= 0) { pinMode(wiring_.enablePin, OUTPUT); digitalWrite(wiring_.enablePin, LOW); }
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, 0);
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override { apply(ramp_.update(nowUs)); }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { apply(ramp_.forceClosed()); }
private:
    void apply(uint8_t level) {
        if (wiring_.enablePin >= 0) digitalWrite(wiring_.enablePin, level > 0 ? HIGH : LOW);
        uint16_t maxDuty = (1u << wiring_.ledcResolutionBits) - 1u;
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, (uint32_t)level * maxDuty / 255u);
    }
    AirRamp ramp_;
    AirModuleWiring wiring_;
    uint32_t pwmFreq_;
};

// 5) DC pump with an H-bridge and configurable direction.
class PumpHBridgeAirController : public IAirController {
public:
    PumpHBridgeAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : ramp_(cfg), wiring_(w), pwmFreq_(cfg.pwmFrequencyHz) {}
    bool begin() override {
        pinMode(wiring_.secondaryPin, OUTPUT);
        ledcSetupPin(wiring_.primaryPin, wiring_.ledcChannel, pwmFreq_, wiring_.ledcResolutionBits);
        digitalWrite(wiring_.secondaryPin, wiring_.forward ? LOW : HIGH);
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, 0);
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override { apply(ramp_.update(nowUs)); }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { apply(ramp_.forceClosed()); }
private:
    void apply(uint8_t level) {
        digitalWrite(wiring_.secondaryPin, wiring_.forward ? LOW : HIGH);
        uint16_t maxDuty = (1u << wiring_.ledcResolutionBits) - 1u;
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, (uint32_t)level * maxDuty / 255u);
    }
    AirRamp ramp_;
    AirModuleWiring wiring_;
    uint32_t pwmFreq_;
};

// 6) Proportional valve driven by PWM (LEDC) or the ESP32 DAC.
class PwmValveAirController : public IAirController {
public:
    PwmValveAirController(const AirConfig& cfg, const AirModuleWiring& w, bool useDac)
        : ramp_(cfg), wiring_(w), useDac_(useDac), pwmFreq_(cfg.pwmFrequencyHz) {}
    bool begin() override {
        if (!useDac_) {
            ledcSetupPin(wiring_.primaryPin, wiring_.ledcChannel, pwmFreq_, wiring_.ledcResolutionBits);
            ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, 0);
        } else {
            writeDac(0);  // GPIO25/26 only, on chips that have a DAC
        }
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override { apply(ramp_.update(nowUs)); }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { apply(ramp_.forceClosed()); }
private:
    void apply(uint8_t level) {
        if (useDac_) {
            writeDac(level);
        } else {
            uint16_t maxDuty = (1u << wiring_.ledcResolutionBits) - 1u;
            ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, (uint32_t)level * maxDuty / 255u);
        }
    }
    // DAC output is only present on ESP32 / ESP32-S2. On chips without a DAC
    // (e.g. ESP32-S3) the call is compiled out; select the PWM path there.
    void writeDac(uint8_t value) {
#if defined(SOC_DAC_SUPPORTED) && SOC_DAC_SUPPORTED
        dacWrite(wiring_.primaryPin, value);
#else
        (void)value;  // no DAC on this target
#endif
    }
    AirRamp ramp_;
    AirModuleWiring wiring_;
    bool useDac_;
    uint32_t pwmFreq_;
};

// 7) Bellows driven by a stepper motor (non-blocking step generation).
class StepperBellowsAirController : public IAirController {
public:
    StepperBellowsAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : ramp_(cfg), wiring_(w) {}
    bool begin() override {
        pinMode(wiring_.stepPin, OUTPUT);
        pinMode(wiring_.dirPin, OUTPUT);
        digitalWrite(wiring_.stepPin, LOW);
        position_ = 0;
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override {
        uint8_t level = ramp_.update(nowUs);
        target_ = (uint32_t)level * wiring_.stepperMaxSteps / 255u;
        // Emit at most one step per ~500us window: non-blocking.
        if (position_ != target_ && (nowUs - lastStepUs_) >= 500) {
            bool up = target_ > position_;
            digitalWrite(wiring_.dirPin, up ? HIGH : LOW);
            digitalWrite(wiring_.stepPin, HIGH);
            delayMicroseconds(3);  // minimum step pulse width, not a control delay
            digitalWrite(wiring_.stepPin, LOW);
            position_ += up ? 1 : -1;
            lastStepUs_ = nowUs;
        }
    }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { ramp_.forceClosed(); target_ = 0; }
private:
    AirRamp ramp_;
    AirModuleWiring wiring_;
    uint32_t position_ = 0;
    uint32_t target_ = 0;
    uint32_t lastStepUs_ = 0;
};

// 8) Permanent external air source enabled by a single digital line.
class ExternalAirController : public IAirController {
public:
    ExternalAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : ramp_(cfg), wiring_(w) {}
    bool begin() override {
        pinMode(wiring_.enablePin, OUTPUT);
        digitalWrite(wiring_.enablePin, LOW);
        return true;
    }
    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void update(uint32_t nowUs) override {
        digitalWrite(wiring_.enablePin, ramp_.update(nowUs) > 0 ? HIGH : LOW);
    }
    void stop() override { ramp_.setDemand(AirDemand{}); }
    void emergencyStop() override { ramp_.forceClosed(); digitalWrite(wiring_.enablePin, LOW); }
private:
    AirRamp ramp_;
    AirModuleWiring wiring_;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // DRIVERS_HARDWAREAIRCONTROLLERS_H
