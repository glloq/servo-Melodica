#ifndef DRIVERS_AIRSTAGES_ESP32_H
#define DRIVERS_AIRSTAGES_ESP32_H

// Concrete ESP32 implementations of the composite air stages. Guarded by ARDUINO
// so the host test build (which exercises the pure logic in AirStages.h with
// mocks) never pulls in these headers.
//
// Every stage checks its pins in begin() (see HardwareCaps) and every source
// tracks how long it has been running, which is what feeds the pump/blower
// overrun watchdog in SafetyManager.
#if defined(ARDUINO)

#include <Arduino.h>
#include <ESP32Servo.h>

#include "AirRunTracker.h"
#include "AirStages.h"
#include "Config.h"
#include "HardwareAirControllers.h"  // reuses the LEDC compatibility shim
#include "HardwareCaps.h"

namespace melodica {

// ---- Main valves ----------------------------------------------------------
class SolenoidMainValve : public IMainValve {
public:
    SolenoidMainValve(int8_t pin, bool activeHigh) : pin_(pin), activeHigh_(activeHigh) {}
    bool begin() override {
        if (!airPinUsable(pin_, "composite main valve")) return false;
        ok_ = true;
        pinMode(pin_, OUTPUT);
        set(false);
        return true;
    }
    void set(bool open) override {
        if (!ok_) return;
        digitalWrite(pin_, (open == activeHigh_) ? HIGH : LOW);
    }
    void update(uint32_t) override {}
private:
    int8_t pin_;
    bool activeHigh_;
    bool ok_ = false;
};

// No gate valve: air path is always open, flow does all the work.
class PassthroughMainValve : public IMainValve {
public:
    bool begin() override { return true; }
    void set(bool) override {}
    void update(uint32_t) override {}
};

// ---- Flow actuators -------------------------------------------------------
class ServoFlowActuator : public IFlowActuator {
public:
    explicit ServoFlowActuator(int8_t pin) : pin_(pin) {}
    bool begin() override {
        if (!airPinUsable(pin_, "composite servo flow")) return false;
        ok_ = true;
        servo_.setPeriodHertz(50);
        servo_.attach(pin_, 500, 2500);
        servo_.write(0);
        return true;
    }
    void setLevel(uint8_t level) override {
        if (ok_) servo_.write(map(level, 0, 255, 0, 180));
    }
    void update(uint32_t) override {}
private:
    int8_t pin_;
    bool ok_ = false;
    Servo servo_;
};

class PwmFlowActuator : public IFlowActuator {
public:
    PwmFlowActuator(int8_t pin, uint8_t ch, uint32_t freq) : pin_(pin), ch_(ch), freq_(freq) {}
    bool begin() override {
        if (!airPinUsable(pin_, "composite PWM flow")) return false;
        ok_ = true;
        ledcSetupPin(pin_, ch_, freq_, 8);
        ledcWritePin(pin_, ch_, 0);
        return true;
    }
    void setLevel(uint8_t level) override { if (ok_) ledcWritePin(pin_, ch_, level); }
    void update(uint32_t) override {}
private:
    int8_t pin_; uint8_t ch_; uint32_t freq_;
    bool ok_ = false;
};

// No proportional flow element: the main valve alone gates the air (on/off).
class NullFlowActuator : public IFlowActuator {
public:
    bool begin() override { return true; }
    void setLevel(uint8_t) override {}
    void update(uint32_t) override {}
};

class DacFlowActuator : public IFlowActuator {
public:
    explicit DacFlowActuator(int8_t pin) : pin_(pin) {}
    bool begin() override {
        if (!GpioCaps::dacCapable(pin_)) {
            LOG_ERROR("Composite flow: GPIO%d has no DAC on this chip", (int)pin_);
            return false;
        }
        ok_ = true;
        write(0);
        return true;
    }
    void setLevel(uint8_t level) override { if (ok_) write(level); }
    void update(uint32_t) override {}
private:
    void write(uint8_t v) {
#if defined(SOC_DAC_SUPPORTED) && SOC_DAC_SUPPORTED
        dacWrite(pin_, v);
#else
        (void)v;
#endif
    }
    int8_t pin_;
    bool ok_ = false;
};

// ---- Air sources ----------------------------------------------------------
// Permanent external air enabled by a digital line (or nothing).
class ExternalSource : public IAirSource {
public:
    explicit ExternalSource(int8_t enablePin) : pin_(enablePin) {}
    bool begin() override {
        if (pin_ >= 0) {
            if (!airPinUsable(pin_, "composite external enable")) return false;
            pinMode(pin_, OUTPUT);
            digitalWrite(pin_, LOW);
        }
        return true;
    }
    void request(bool active, uint8_t) override { active_ = active; }
    void update(uint32_t nowUs, uint8_t) override {
        if (pin_ >= 0) digitalWrite(pin_, active_ ? HIGH : LOW);
        run_.update(nowUs, active_);
    }
    void stop() override { active_ = false; run_.reset(); if (pin_ >= 0) digitalWrite(pin_, LOW); }
    bool isRunning() const override { return run_.running(); }
    uint32_t runtimeMs() const override { return run_.runtimeMs(); }
private:
    int8_t pin_;
    bool active_ = false;
    AirRunTracker run_;
};

// DC blower/fan via PWM; follows the shaped level while sound is requested.
class BlowerSource : public IAirSource {
public:
    BlowerSource(int8_t pin, uint8_t ch, uint32_t freq) : pin_(pin), ch_(ch), freq_(freq) {}
    bool begin() override {
        if (!airPinUsable(pin_, "composite blower")) return false;
        ok_ = true;
        ledcSetupPin(pin_, ch_, freq_, 8);
        ledcWritePin(pin_, ch_, 0);
        return true;
    }
    void request(bool active, uint8_t demand) override { active_ = active; demand_ = demand; }
    void update(uint32_t nowUs, uint8_t) override {
        uint8_t duty = active_ ? demand_ : 0;
        if (ok_) ledcWritePin(pin_, ch_, duty);
        run_.update(nowUs, duty > 0);
    }
    void stop() override { active_ = false; demand_ = 0; run_.reset(); if (ok_) ledcWritePin(pin_, ch_, 0); }
    bool isRunning() const override { return run_.running(); }
    uint32_t runtimeMs() const override { return run_.runtimeMs(); }
private:
    int8_t pin_; uint8_t ch_; uint32_t freq_;
    bool ok_ = false;
    bool active_ = false; uint8_t demand_ = 0;
    AirRunTracker run_;
};

// DC pump with an H-bridge; direction configurable, runs while sound requested.
class PumpSource : public IAirSource {
public:
    PumpSource(int8_t pwmPin, int8_t dirPin, uint8_t ch, uint32_t freq, bool forward)
        : pwm_(pwmPin), dir_(dirPin), ch_(ch), freq_(freq), forward_(forward) {}
    bool begin() override {
        if (!airPinUsable(pwm_, "composite pump PWM")) return false;
        if (!airPinUsable(dir_, "composite pump direction")) return false;
        ok_ = true;
        pinMode(dir_, OUTPUT);
        digitalWrite(dir_, forward_ ? LOW : HIGH);
        ledcSetupPin(pwm_, ch_, freq_, 8);
        ledcWritePin(pwm_, ch_, 0);
        return true;
    }
    void request(bool active, uint8_t demand) override { active_ = active; demand_ = demand; }
    void update(uint32_t nowUs, uint8_t) override {
        uint8_t duty = active_ ? demand_ : 0;
        if (ok_) {
            digitalWrite(dir_, forward_ ? LOW : HIGH);
            ledcWritePin(pwm_, ch_, duty);
        }
        run_.update(nowUs, duty > 0);
    }
    void stop() override { active_ = false; demand_ = 0; run_.reset(); if (ok_) ledcWritePin(pwm_, ch_, 0); }
    bool isRunning() const override { return run_.running(); }
    uint32_t runtimeMs() const override { return run_.runtimeMs(); }
private:
    int8_t pwm_, dir_; uint8_t ch_; uint32_t freq_; bool forward_;
    bool ok_ = false;
    bool active_ = false; uint8_t demand_ = 0;
    AirRunTracker run_;
};

// Pressurised reservoir kept at a set-point by a pump/blower, regulated with the
// pressure sensor (bang-bang via ReservoirRegulator). The main valve gates output.
//
// This is the one source that can run with no notes playing, so its run tracker
// is what makes SafetyConfig::maxPumpRunMs meaningful: a leaking tank, a dead
// sensor or an unreachable set-point trips PumpOverrun instead of running the
// pump until something melts. CompositeAirController additionally refuses to
// start this source at all without a valid sensor.
class ReservoirSource : public IAirSource {
public:
    ReservoirSource(int8_t pwmPin, uint8_t ch, uint32_t freq, uint8_t target, uint8_t hyst)
        : pwm_(pwmPin), ch_(ch), freq_(freq), target_(target), hyst_(hyst) {}
    bool begin() override {
        if (!airPinUsable(pwm_, "reservoir pump")) return false;
        ok_ = true;
        ledcSetupPin(pwm_, ch_, freq_, 8);
        ledcWritePin(pwm_, ch_, 0);
        return true;
    }
    void request(bool, uint8_t) override {}  // reservoir fills regardless of notes
    void update(uint32_t nowUs, uint8_t pressureNorm) override {
        pumpOn_ = ReservoirRegulator::pumpShouldRun(pumpOn_, pressureNorm, target_, hyst_);
        if (ok_) ledcWritePin(pwm_, ch_, pumpOn_ ? 255 : 0);
        run_.update(nowUs, pumpOn_);
    }
    void stop() override { pumpOn_ = false; run_.reset(); if (ok_) ledcWritePin(pwm_, ch_, 0); }
    bool isRunning() const override { return run_.running(); }
    uint32_t runtimeMs() const override { return run_.runtimeMs(); }
private:
    int8_t pwm_; uint8_t ch_; uint32_t freq_; uint8_t target_, hyst_;
    bool ok_ = false;
    bool pumpOn_ = false;
    AirRunTracker run_;
};

// ---- Pressure sensor ------------------------------------------------------
class AnalogPressureSensor : public IPressureSensor {
public:
    AnalogPressureSensor(int8_t pin, uint16_t minRaw, uint16_t maxRaw)
        : pin_(pin), min_(minRaw), max_(maxRaw) {}
    bool begin() override {
        if (!valid()) {
            LOG_ERROR("Pressure sensor: GPIO%d / raw range %u..%u unusable", (int)pin_,
                      (unsigned)min_, (unsigned)max_);
            return false;
        }
        pinMode(pin_, INPUT);
        return true;
    }
    // An ADC pin and a non-degenerate raw range are both required: without them
    // every reading would be 0, which reads as "empty tank" forever.
    bool valid() const override { return GpioCaps::adcCapable(pin_) && max_ > min_; }
    uint8_t readNormalized() override {
        if (!valid()) return 0;
        int raw = analogRead(pin_);
        if (raw <= min_) return 0;
        if (raw >= max_) return 255;
        return static_cast<uint8_t>((static_cast<uint32_t>(raw - min_) * 255u) / (max_ - min_));
    }
private:
    int8_t pin_; uint16_t min_, max_;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // DRIVERS_AIRSTAGES_ESP32_H
