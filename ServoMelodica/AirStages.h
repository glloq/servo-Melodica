#ifndef CORE_AIRSTAGES_H
#define CORE_AIRSTAGES_H

#include <cstdint>

#include "AirDemand.h"
#include "AirRamp.h"
#include "Config.h"
#include "IAirController.h"

// Composable air system — four independent stages behind small interfaces so the
// orchestration logic is platform-independent and host-testable, while the
// concrete actuators live in AirStages_ESP32.h.
//
//   IAirSource  ──►  IMainValve  ──►  IFlowActuator  ──►  melodica
//        ▲
//   IPressureSensor (optional, for reservoir regulation)
namespace melodica {

// On/off main gate valve (e.g. solenoid), or a pass-through when there is none.
class IMainValve {
public:
    virtual ~IMainValve() = default;
    virtual bool begin() = 0;
    virtual void set(bool open) = 0;
    virtual void update(uint32_t nowUs) = 0;
};

// Proportional flow element (servo / PWM / DAC), 0..255.
class IFlowActuator {
public:
    virtual ~IFlowActuator() = default;
    virtual bool begin() = 0;
    virtual void setLevel(uint8_t level) = 0;
    virtual void update(uint32_t nowUs) = 0;
};

// Air supply (blower / pump / reservoir / permanent external). `pressureNorm` is
// the current normalised pressure (0..255, 255 when no sensor).
class IAirSource {
public:
    virtual ~IAirSource() = default;
    virtual bool begin() = 0;
    virtual void request(bool active, uint8_t demand) = 0;
    virtual void update(uint32_t nowUs, uint8_t pressureNorm) = 0;
    virtual void stop() = 0;

    // Duty supervision. A source can run on its own schedule (a reservoir keeps
    // filling with no notes playing at all), so the overrun watchdog has to ask
    // the source itself, not the note-driven ramp.
    virtual bool isRunning() const { return false; }
    virtual uint32_t runtimeMs() const { return 0; }
};

// Optional pressure sensor, normalised to 0..255.
class IPressureSensor {
public:
    virtual ~IPressureSensor() = default;
    virtual bool begin() = 0;
    virtual uint8_t readNormalized() = 0;
    // False when the sensor cannot produce a meaningful reading (no ADC pin, or
    // a calibration where max <= min, which would report "0 bar" forever and
    // make a regulator run its pump without end).
    virtual bool valid() const { return true; }
};

// Pure bang-bang regulator for a pressurised reservoir: keep a pump/blower on
// until the target is reached, off until it drops below target-hysteresis. Kept
// separate so it is unit-tested without any hardware.
class ReservoirRegulator {
public:
    // Returns whether the pump should be ON given the current pressure.
    static bool pumpShouldRun(bool currentlyOn, uint8_t pressure, uint8_t target,
                              uint8_t hysteresis) {
        uint8_t low = target > hysteresis ? static_cast<uint8_t>(target - hysteresis) : 0;
        if (currentlyOn) {
            return pressure < target;     // keep filling until we reach target
        }
        return pressure < low;            // start again once it drops below band
    }
};

// Orchestrates the four stages into a single IAirController. All timing lives in
// the shared AirRamp (flow shaping) and the stages; this class is pure logic.
//
// The shaped level drives BOTH the flow element and the source, so attack/
// release actually shape the sound on a "blower + no flow valve" build where the
// blower IS the only proportional element. The source is fed the pre-inversion
// level: `inverted` describes how the flow valve is wired, and must not run a
// blower at full speed when the instrument is silent.
class CompositeAirController : public IAirController {
public:
    CompositeAirController(const AirConfig& cfg, IMainValve& valve, IFlowActuator& flow,
                           IAirSource& source, IPressureSensor* sensor)
        : cfg_(cfg), ramp_(cfg), valve_(valve), flow_(flow), source_(source),
          sensor_(sensor) {}

    bool begin() override {
        ramp_.setConfig(cfg_);
        // A regulated reservoir without a usable sensor would run its pump on a
        // permanent "pressure = 0" reading. Refuse to start it at all.
        if (cfg_.composite.source == SourceKind::Reservoir &&
            (sensor_ == nullptr || !sensor_->valid())) {
            fault_ = AirFault::InvalidSensor;
            source_.stop();
            return false;
        }
        bool ok = valve_.begin();
        ok = flow_.begin() && ok;
        ok = source_.begin() && ok;
        if (sensor_) ok = sensor_->begin() && ok;
        fault_ = ok ? AirFault::None : AirFault::NotInitialised;
        return ok;
    }

    void setDemand(const AirDemand& d) override {
        demand_ = d;
        ramp_.setDemand(d);
    }

    void update(uint32_t nowUs) override {
        if (fault_ != AirFault::None) return;  // never drive a faulted air system

        uint8_t level = ramp_.update(nowUs);        // shaped flow level (0..255)
        uint8_t sourceLevel = ramp_.rawLevel();     // same shape, never inverted
        // The main valve opens when real sound is requested; the flow element
        // then modulates how much air passes.
        bool open = demand_.soundRequested && demand_.expression >= cfg_.startThreshold;
        uint8_t pressure = (sensor_ && sensor_->valid()) ? sensor_->readNormalized() : 255;

        valve_.set(open);
        flow_.setLevel(level);
        source_.request(open, sourceLevel);
        source_.update(nowUs, pressure);
        valve_.update(nowUs);
        flow_.update(nowUs);
    }

    void stop() override {
        demand_ = AirDemand{};
        ramp_.setDemand(AirDemand{});
    }

    void emergencyStop() override {
        uint8_t closed = ramp_.forceClosed();
        demand_ = AirDemand{};
        valve_.set(false);
        flow_.setLevel(closed);
        source_.stop();
    }

    // Running == any stage is moving air. runtimeMs is the longest current run,
    // so a reservoir pump filling for two minutes trips the overrun watchdog even
    // though no note has been played.
    bool isRunning() const override { return ramp_.isRunning() || source_.isRunning(); }
    uint32_t runtimeMs() const override {
        uint32_t a = ramp_.runtimeMs();
        uint32_t b = source_.runtimeMs();
        return a > b ? a : b;
    }
    bool healthy() const override { return fault_ == AirFault::None; }
    AirFault fault() const override { return fault_; }

private:
    const AirConfig& cfg_;
    AirRamp ramp_;
    IMainValve& valve_;
    IFlowActuator& flow_;
    IAirSource& source_;
    IPressureSensor* sensor_;
    AirDemand demand_{};
    AirFault fault_ = AirFault::NotInitialised;
};

}  // namespace melodica

#endif  // CORE_AIRSTAGES_H
