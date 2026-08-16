#ifndef DRIVERS_HARDWAREAIRCONTROLLERS_H
#define DRIVERS_HARDWAREAIRCONTROLLERS_H

// Concrete air modules that drive real ESP32 hardware. Each one delegates the
// demand->level shaping and the duty supervision to AirControllerBase/AirRamp
// (thresholds, ramps, precharge, inversion, run-time tracking) and only
// implements how a level of 0..255 is applied to its actuator. Guarded by
// ARDUINO so the host test build never pulls in ESP32 headers.
//
// Every module validates its pins in begin(): an unusable GPIO (not bonded out,
// SPI-flash, input-only, no DAC) latches an AirFault, keeps begin() false and
// makes update() a no-op, so a mis-configured air system stays silent instead of
// half-driving hardware. SafetyManager turns that fault into a latched stop.
#if defined(ARDUINO)

#include <Arduino.h>
#include <ESP32Servo.h>

#include "AirControllerBase.h"
#include "AirModuleConfig.h"
#include "HardwareCaps.h"
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

// Shared pin check: logs and reports which output was rejected.
inline bool airPinUsable(int8_t pin, const char* what) {
    if (GpioCaps::outputCapable(pin)) return true;
    LOG_ERROR("Air module: GPIO%d cannot drive '%s' on this chip", (int)pin, what);
    return false;
}

// 1) Proportional air valve driven by a hobby servo.
class ServoValveAirController : public AirControllerBase {
public:
    ServoValveAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : AirControllerBase(cfg), wiring_(w) {}
    bool begin() override {
        if (!airPinUsable(wiring_.primaryPin, "servo valve")) return fail(AirFault::InvalidPin);
        servo_.setPeriodHertz(50);
        servo_.attach(wiring_.primaryPin, 500, 2500);
        servo_.write(0);
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        apply(ramp_.update(nowUs));
    }
    void emergencyStop() override {
        uint8_t closed = ramp_.forceClosed();
        if (healthy()) apply(closed);
    }
private:
    void apply(uint8_t level) { servo_.write(map(level, 0, 255, 0, 180)); }
    AirModuleWiring wiring_;
    Servo servo_;
};

// 2) On/off solenoid valve.
class SolenoidAirController : public AirControllerBase {
public:
    SolenoidAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : AirControllerBase(cfg), wiring_(w) {}
    bool begin() override {
        if (!airPinUsable(wiring_.primaryPin, "solenoid")) return fail(AirFault::InvalidPin);
        pinMode(wiring_.primaryPin, OUTPUT);
        digitalWrite(wiring_.primaryPin, LOW);
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        apply(ramp_.update(nowUs));
    }
    void emergencyStop() override {
        uint8_t closed = ramp_.forceClosed();
        if (healthy()) apply(closed);
    }
private:
    void apply(uint8_t level) {
        digitalWrite(wiring_.primaryPin, level > 0 ? HIGH : LOW);
    }
    AirModuleWiring wiring_;
};

// 3) Several solenoids opened in flow steps.
class SteppedSolenoidAirController : public AirControllerBase {
public:
    SteppedSolenoidAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : AirControllerBase(cfg), wiring_(w) {}
    bool begin() override {
        if (wiring_.stageCount == 0 || wiring_.stageCount > 4) {
            LOG_ERROR("Stepped solenoid: stageCount %u out of range", wiring_.stageCount);
            return fail(AirFault::InvalidPin);
        }
        for (uint8_t i = 0; i < wiring_.stageCount; ++i) {
            if (!airPinUsable(wiring_.stagePins[i], "solenoid stage")) {
                return fail(AirFault::InvalidPin);
            }
        }
        for (uint8_t i = 0; i < wiring_.stageCount; ++i) {
            pinMode(wiring_.stagePins[i], OUTPUT);
            digitalWrite(wiring_.stagePins[i], LOW);
        }
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        apply(ramp_.update(nowUs));
    }
    void emergencyStop() override {
        uint8_t closed = ramp_.forceClosed();
        if (healthy()) apply(closed);
    }
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
    AirModuleWiring wiring_;
};

// 4) DC blower / fan via LEDC PWM.
class BlowerPwmAirController : public AirControllerBase {
public:
    BlowerPwmAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : AirControllerBase(cfg), wiring_(w), pwmFreq_(cfg.pwmFrequencyHz) {}
    bool begin() override {
        if (!airPinUsable(wiring_.primaryPin, "blower PWM")) return fail(AirFault::InvalidPin);
        if (wiring_.enablePin >= 0 && !airPinUsable(wiring_.enablePin, "blower enable")) {
            return fail(AirFault::InvalidPin);
        }
        ledcSetupPin(wiring_.primaryPin, wiring_.ledcChannel, pwmFreq_, wiring_.ledcResolutionBits);
        if (wiring_.enablePin >= 0) { pinMode(wiring_.enablePin, OUTPUT); digitalWrite(wiring_.enablePin, LOW); }
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, 0);
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        apply(ramp_.update(nowUs));
    }
    void emergencyStop() override {
        uint8_t closed = ramp_.forceClosed();
        if (healthy()) apply(closed);
    }
private:
    void apply(uint8_t level) {
        if (wiring_.enablePin >= 0) digitalWrite(wiring_.enablePin, level > 0 ? HIGH : LOW);
        uint16_t maxDuty = (1u << wiring_.ledcResolutionBits) - 1u;
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, (uint32_t)level * maxDuty / 255u);
    }
    AirModuleWiring wiring_;
    uint32_t pwmFreq_;
};

// 5) DC pump with an H-bridge and configurable direction.
class PumpHBridgeAirController : public AirControllerBase {
public:
    PumpHBridgeAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : AirControllerBase(cfg), wiring_(w), pwmFreq_(cfg.pwmFrequencyHz) {}
    bool begin() override {
        if (!airPinUsable(wiring_.primaryPin, "pump PWM")) return fail(AirFault::InvalidPin);
        if (!airPinUsable(wiring_.secondaryPin, "pump direction")) return fail(AirFault::InvalidPin);
        pinMode(wiring_.secondaryPin, OUTPUT);
        ledcSetupPin(wiring_.primaryPin, wiring_.ledcChannel, pwmFreq_, wiring_.ledcResolutionBits);
        digitalWrite(wiring_.secondaryPin, wiring_.forward ? LOW : HIGH);
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, 0);
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        apply(ramp_.update(nowUs));
    }
    void emergencyStop() override {
        uint8_t closed = ramp_.forceClosed();
        if (healthy()) apply(closed);
    }
private:
    void apply(uint8_t level) {
        digitalWrite(wiring_.secondaryPin, wiring_.forward ? LOW : HIGH);
        uint16_t maxDuty = (1u << wiring_.ledcResolutionBits) - 1u;
        ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, (uint32_t)level * maxDuty / 255u);
    }
    AirModuleWiring wiring_;
    uint32_t pwmFreq_;
};

// 6) Proportional valve driven by PWM (LEDC) or the ESP32 DAC.
class PwmValveAirController : public AirControllerBase {
public:
    PwmValveAirController(const AirConfig& cfg, const AirModuleWiring& w, bool useDac)
        : AirControllerBase(cfg), wiring_(w), useDac_(useDac), pwmFreq_(cfg.pwmFrequencyHz) {}
    bool begin() override {
        if (!airPinUsable(wiring_.primaryPin, "proportional valve")) {
            return fail(AirFault::InvalidPin);
        }
        if (useDac_ && !GpioCaps::dacCapable(wiring_.primaryPin)) {
            LOG_ERROR("Air module: GPIO%d has no DAC on this chip", (int)wiring_.primaryPin);
            return fail(AirFault::InvalidPin);
        }
        if (!useDac_) {
            ledcSetupPin(wiring_.primaryPin, wiring_.ledcChannel, pwmFreq_, wiring_.ledcResolutionBits);
            ledcWritePin(wiring_.primaryPin, wiring_.ledcChannel, 0);
        } else {
            writeDac(0);  // GPIO25/26 only, on chips that have a DAC
        }
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        apply(ramp_.update(nowUs));
    }
    void emergencyStop() override {
        uint8_t closed = ramp_.forceClosed();
        if (healthy()) apply(closed);
    }
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
    // (e.g. ESP32-S3) begin() already refused the configuration.
    void writeDac(uint8_t value) {
#if defined(SOC_DAC_SUPPORTED) && SOC_DAC_SUPPORTED
        dacWrite(wiring_.primaryPin, value);
#else
        (void)value;  // no DAC on this target
#endif
    }
    AirModuleWiring wiring_;
    bool useDac_;
    uint32_t pwmFreq_;
};

// 7) Bellows driven by a stepper motor (non-blocking step generation).
//
// Prototype-grade: open-loop, no homing and no end-stops (see README). It
// assumes the bellows is at its rest position at power-up and refuses to travel
// outside [0, stepperMaxSteps]; a real instrument should add limit switches.
class StepperBellowsAirController : public AirControllerBase {
public:
    StepperBellowsAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : AirControllerBase(cfg), wiring_(w) {}
    bool begin() override {
        if (!airPinUsable(wiring_.stepPin, "stepper step")) return fail(AirFault::InvalidPin);
        if (!airPinUsable(wiring_.dirPin, "stepper dir")) return fail(AirFault::InvalidPin);
        pinMode(wiring_.stepPin, OUTPUT);
        pinMode(wiring_.dirPin, OUTPUT);
        digitalWrite(wiring_.stepPin, LOW);
        position_ = 0;
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        uint8_t level = ramp_.update(nowUs);
        target_ = (uint32_t)level * wiring_.stepperMaxSteps / 255u;
        // Emit at most one step per ~500us window: non-blocking.
        if (position_ != target_ && (nowUs - lastStepUs_) >= 500) {
            bool up = target_ > position_;
            if (up && position_ >= wiring_.stepperMaxSteps) return;  // software end-stop
            if (!up && position_ == 0) return;
            digitalWrite(wiring_.dirPin, up ? HIGH : LOW);
            digitalWrite(wiring_.stepPin, HIGH);
            delayMicroseconds(3);  // minimum step pulse width, not a control delay
            digitalWrite(wiring_.stepPin, LOW);
            position_ += up ? 1 : -1;
            lastStepUs_ = nowUs;
        }
    }
    void emergencyStop() override { ramp_.forceClosed(); target_ = 0; }
private:
    AirModuleWiring wiring_;
    uint32_t position_ = 0;
    uint32_t target_ = 0;
    uint32_t lastStepUs_ = 0;
};

// 8) Permanent external air source enabled by a single digital line.
class ExternalAirController : public AirControllerBase {
public:
    ExternalAirController(const AirConfig& cfg, const AirModuleWiring& w)
        : AirControllerBase(cfg), wiring_(w) {}
    bool begin() override {
        if (!airPinUsable(wiring_.enablePin, "external air enable")) {
            return fail(AirFault::InvalidPin);
        }
        pinMode(wiring_.enablePin, OUTPUT);
        digitalWrite(wiring_.enablePin, LOW);
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override {
        if (!healthy()) return;
        digitalWrite(wiring_.enablePin, ramp_.update(nowUs) > 0 ? HIGH : LOW);
    }
    void emergencyStop() override {
        ramp_.forceClosed();
        if (healthy()) digitalWrite(wiring_.enablePin, LOW);
    }
private:
    AirModuleWiring wiring_;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // DRIVERS_HARDWAREAIRCONTROLLERS_H
