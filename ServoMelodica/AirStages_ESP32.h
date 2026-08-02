#ifndef DRIVERS_AIRSTAGES_ESP32_H
#define DRIVERS_AIRSTAGES_ESP32_H

// Concrete ESP32 implementations of the composite air stages. Guarded by ARDUINO
// so the host test build (which exercises the pure logic in AirStages.h with
// mocks) never pulls in these headers.
#if defined(ARDUINO)

#include <Arduino.h>
#include <ESP32Servo.h>

#include "AirStages.h"
#include "Config.h"
#include "HardwareAirControllers.h"  // reuses the LEDC compatibility shim

namespace melodica {

// ---- Main valves ----------------------------------------------------------
class SolenoidMainValve : public IMainValve {
public:
    SolenoidMainValve(int8_t pin, bool activeHigh) : pin_(pin), activeHigh_(activeHigh) {}
    bool begin() override { pinMode(pin_, OUTPUT); set(false); return true; }
    void set(bool open) override { digitalWrite(pin_, (open == activeHigh_) ? HIGH : LOW); }
    void update(uint32_t) override {}
private:
    int8_t pin_;
    bool activeHigh_;
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
    bool begin() override { servo_.setPeriodHertz(50); servo_.attach(pin_, 500, 2500); servo_.write(0); return true; }
    void setLevel(uint8_t level) override { servo_.write(map(level, 0, 255, 0, 180)); }
    void update(uint32_t) override {}
private:
    int8_t pin_;
    Servo servo_;
};

class PwmFlowActuator : public IFlowActuator {
public:
    PwmFlowActuator(int8_t pin, uint8_t ch, uint32_t freq) : pin_(pin), ch_(ch), freq_(freq) {}
    bool begin() override { ledcSetupPin(pin_, ch_, freq_, 8); ledcWritePin(pin_, ch_, 0); return true; }
    void setLevel(uint8_t level) override { ledcWritePin(pin_, ch_, level); }
    void update(uint32_t) override {}
private:
    int8_t pin_; uint8_t ch_; uint32_t freq_;
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
    bool begin() override { write(0); return true; }
    void setLevel(uint8_t level) override { write(level); }
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
};

// ---- Air sources ----------------------------------------------------------
// Permanent external air enabled by a digital line (or nothing).
class ExternalSource : public IAirSource {
public:
    explicit ExternalSource(int8_t enablePin) : pin_(enablePin) {}
    bool begin() override { if (pin_ >= 0) { pinMode(pin_, OUTPUT); digitalWrite(pin_, LOW); } return true; }
    void request(bool active, uint8_t) override { active_ = active; }
    void update(uint32_t, uint8_t) override { if (pin_ >= 0) digitalWrite(pin_, active_ ? HIGH : LOW); }
    void stop() override { active_ = false; if (pin_ >= 0) digitalWrite(pin_, LOW); }
private:
    int8_t pin_;
    bool active_ = false;
};

// DC blower/fan via PWM; runs proportionally to demand while sound is requested.
class BlowerSource : public IAirSource {
public:
    BlowerSource(int8_t pin, uint8_t ch, uint32_t freq) : pin_(pin), ch_(ch), freq_(freq) {}
    bool begin() override { ledcSetupPin(pin_, ch_, freq_, 8); ledcWritePin(pin_, ch_, 0); return true; }
    void request(bool active, uint8_t demand) override { active_ = active; demand_ = demand; }
    void update(uint32_t, uint8_t) override { ledcWritePin(pin_, ch_, active_ ? demand_ : 0); }
    void stop() override { active_ = false; ledcWritePin(pin_, ch_, 0); }
private:
    int8_t pin_; uint8_t ch_; uint32_t freq_;
    bool active_ = false; uint8_t demand_ = 0;
};

// DC pump with an H-bridge; direction configurable, runs while sound requested.
class PumpSource : public IAirSource {
public:
    PumpSource(int8_t pwmPin, int8_t dirPin, uint8_t ch, uint32_t freq, bool forward)
        : pwm_(pwmPin), dir_(dirPin), ch_(ch), freq_(freq), forward_(forward) {}
    bool begin() override { pinMode(dir_, OUTPUT); digitalWrite(dir_, forward_ ? LOW : HIGH); ledcSetupPin(pwm_, ch_, freq_, 8); ledcWritePin(pwm_, ch_, 0); return true; }
    void request(bool active, uint8_t demand) override { active_ = active; demand_ = demand; }
    void update(uint32_t, uint8_t) override { digitalWrite(dir_, forward_ ? LOW : HIGH); ledcWritePin(pwm_, ch_, active_ ? demand_ : 0); }
    void stop() override { active_ = false; ledcWritePin(pwm_, ch_, 0); }
private:
    int8_t pwm_, dir_; uint8_t ch_; uint32_t freq_; bool forward_;
    bool active_ = false; uint8_t demand_ = 0;
};

// Pressurised reservoir kept at a set-point by a pump/blower, regulated with the
// pressure sensor (bang-bang via ReservoirRegulator). The main valve gates output.
class ReservoirSource : public IAirSource {
public:
    ReservoirSource(int8_t pwmPin, uint8_t ch, uint32_t freq, uint8_t target, uint8_t hyst)
        : pwm_(pwmPin), ch_(ch), freq_(freq), target_(target), hyst_(hyst) {}
    bool begin() override { ledcSetupPin(pwm_, ch_, freq_, 8); ledcWritePin(pwm_, ch_, 0); return true; }
    void request(bool, uint8_t) override {}  // reservoir fills regardless of notes
    void update(uint32_t, uint8_t pressureNorm) override {
        pumpOn_ = ReservoirRegulator::pumpShouldRun(pumpOn_, pressureNorm, target_, hyst_);
        ledcWritePin(pwm_, ch_, pumpOn_ ? 255 : 0);
    }
    void stop() override { pumpOn_ = false; ledcWritePin(pwm_, ch_, 0); }
private:
    int8_t pwm_; uint8_t ch_; uint32_t freq_; uint8_t target_, hyst_;
    bool pumpOn_ = false;
};

// ---- Pressure sensor ------------------------------------------------------
class AnalogPressureSensor : public IPressureSensor {
public:
    AnalogPressureSensor(int8_t pin, uint16_t minRaw, uint16_t maxRaw)
        : pin_(pin), min_(minRaw), max_(maxRaw) {}
    bool begin() override { pinMode(pin_, INPUT); return true; }
    uint8_t readNormalized() override {
        int raw = analogRead(pin_);
        if (max_ <= min_) return 0;
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
