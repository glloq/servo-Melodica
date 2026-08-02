#ifndef DRIVERS_AIRCONTROLLERFACTORY_H
#define DRIVERS_AIRCONTROLLERFACTORY_H

// Runtime selection of the air module.
//
// By default (no -DAIR_* flag, e.g. an Arduino IDE build) ALL air modules are
// compiled in and the actual one is chosen at runtime from BoardConfig::air.type
// — this is what lets the web UI / serial console configure "any type of air
// system" without reflashing. A PlatformIO profile that defines a single
// -DAIR_<X> flag compiles only that module for a lean binary and always uses it.
//
// The selected controller is constructed once at setup() via placement-new into
// caller-provided static storage: no heap, nothing in the real-time path.

#if !defined(AIR_ALL) && !defined(AIR_SERVO_VALVE) && !defined(AIR_SOLENOID) &&    \
    !defined(AIR_STEPPED_SOLENOID) && !defined(AIR_BLOWER_PWM) &&                  \
    !defined(AIR_PUMP_HBRIDGE) && !defined(AIR_PWM_VALVE) &&                       \
    !defined(AIR_STEPPER_BELLOWS) && !defined(AIR_EXTERNAL) && !defined(AIR_SIMULATION)
#define AIR_ALL
#endif

#include <new>

#include "Config.h"
#include "IAirController.h"
#include "Log.h"
#include "AirStages.h"
#include "SimulationAirController.h"
#if defined(AIR_ALL) || !defined(AIR_SIMULATION)
#include "HardwareAirControllers.h"
#endif
#if defined(ARDUINO)
#include "AirStages_ESP32.h"
#endif

namespace melodica {

// Storage large enough (and correctly aligned) for the biggest air module.
// static_assert in createAirController() guards against a module outgrowing it.
struct alignas(16) AirControllerStorage {
    unsigned char bytes[320];
};

// Storage for the four composite stages + the composite controller.
struct CompositeAirStorage {
    AirControllerStorage valve, flow, source, sensor, controller;
};

// Placement-new an object into a storage buffer with a compile-time size check.
template <class T, class... Args>
inline T* placeIn(AirControllerStorage& s, Args&&... args) {
    static_assert(sizeof(T) <= sizeof(AirControllerStorage),
                  "AirControllerStorage too small for this object");
    return new (s.bytes) T(static_cast<Args&&>(args)...);
}
template <class T, class... Args>
inline IAirController* placeAir(AirControllerStorage& s, Args&&... args) {
    return placeIn<T>(s, static_cast<Args&&>(args)...);
}

#if defined(ARDUINO)
// Build a 4-stage composite air system from config (source -> valve -> flow +
// optional sensor). Each stage is placement-new'd into its own static buffer.
inline IAirController* buildComposite(const BoardConfig& cfg, CompositeAirStorage& s) {
    const CompositeAirConfig& c = cfg.air.composite;
    const uint32_t freq = cfg.air.pwmFrequencyHz;

    IMainValve* valve;
    if (c.mainValve == MainValveKind::Solenoid)
        valve = placeIn<SolenoidMainValve>(s.valve, c.mainValvePin, c.mainValveActiveHigh);
    else
        valve = placeIn<PassthroughMainValve>(s.valve);

    IFlowActuator* flow;
    switch (c.flow) {
        case FlowKind::Servo: flow = placeIn<ServoFlowActuator>(s.flow, c.flowPin); break;
        case FlowKind::Pwm: flow = placeIn<PwmFlowActuator>(s.flow, c.flowPin, c.flowLedcChannel, freq); break;
        case FlowKind::Dac: flow = placeIn<DacFlowActuator>(s.flow, c.flowPin); break;
        default: flow = placeIn<NullFlowActuator>(s.flow); break;
    }

    IAirSource* source;
    switch (c.source) {
        case SourceKind::Blower: source = placeIn<BlowerSource>(s.source, c.sourcePwmPin, c.sourceLedcChannel, freq); break;
        case SourceKind::Pump: source = placeIn<PumpSource>(s.source, c.sourcePwmPin, c.sourceDirPin, c.sourceLedcChannel, freq, c.sourceForward); break;
        case SourceKind::Reservoir: source = placeIn<ReservoirSource>(s.source, c.sourcePwmPin, c.sourceLedcChannel, freq, c.targetPressure, c.pressureHysteresis); break;
        default: source = placeIn<ExternalSource>(s.source, c.sourceEnablePin); break;
    }

    IPressureSensor* sensor = nullptr;
    if (c.sensorEnabled)
        sensor = placeIn<AnalogPressureSensor>(s.sensor, c.sensorPin, c.sensorMinRaw, c.sensorMaxRaw);

    return placeIn<CompositeAirController>(s.controller, cfg.air, *valve, *flow, *source, sensor);
}
#endif  // ARDUINO

inline IAirController* createAirController(const BoardConfig& cfg,
                                           AirControllerStorage& storage,
                                           CompositeAirStorage& composite) {
    const AirConfig& a = cfg.air;

#if defined(ARDUINO)
    if (a.type == AirSystemType::Composite) return buildComposite(cfg, composite);
#else
    (void)composite;
#endif

#if defined(AIR_ALL)
    // Full build: honour the runtime-selected type.
    switch (a.type) {
#if defined(ARDUINO)
        case AirSystemType::ServoValve:
            return placeAir<ServoValveAirController>(storage, a, a.wiring);
        case AirSystemType::Solenoid:
            return placeAir<SolenoidAirController>(storage, a, a.wiring);
        case AirSystemType::SteppedSolenoid:
            return placeAir<SteppedSolenoidAirController>(storage, a, a.wiring);
        case AirSystemType::BlowerPwm:
            return placeAir<BlowerPwmAirController>(storage, a, a.wiring);
        case AirSystemType::PumpHBridge:
            return placeAir<PumpHBridgeAirController>(storage, a, a.wiring);
        case AirSystemType::PwmValve:
            return placeAir<PwmValveAirController>(storage, a, a.wiring, false);
        case AirSystemType::PwmValveDac:
            return placeAir<PwmValveAirController>(storage, a, a.wiring, true);
        case AirSystemType::StepperBellows:
            return placeAir<StepperBellowsAirController>(storage, a, a.wiring);
        case AirSystemType::ExternalAir:
            return placeAir<ExternalAirController>(storage, a, a.wiring);
#endif
        case AirSystemType::Simulation:
        default:
            return placeAir<SimulationAirController>(storage, a);
    }
#elif defined(AIR_SERVO_VALVE)
    return placeAir<ServoValveAirController>(storage, a, a.wiring);
#elif defined(AIR_SOLENOID)
    return placeAir<SolenoidAirController>(storage, a, a.wiring);
#elif defined(AIR_STEPPED_SOLENOID)
    return placeAir<SteppedSolenoidAirController>(storage, a, a.wiring);
#elif defined(AIR_BLOWER_PWM)
    return placeAir<BlowerPwmAirController>(storage, a, a.wiring);
#elif defined(AIR_PUMP_HBRIDGE)
    return placeAir<PumpHBridgeAirController>(storage, a, a.wiring);
#elif defined(AIR_PWM_VALVE)
    return placeAir<PwmValveAirController>(storage, a, a.wiring, false);
#elif defined(AIR_STEPPER_BELLOWS)
    return placeAir<StepperBellowsAirController>(storage, a, a.wiring);
#elif defined(AIR_EXTERNAL)
    return placeAir<ExternalAirController>(storage, a, a.wiring);
#else  // AIR_SIMULATION
    return placeAir<SimulationAirController>(storage, a);
#endif
}

}  // namespace melodica

#endif  // DRIVERS_AIRCONTROLLERFACTORY_H
