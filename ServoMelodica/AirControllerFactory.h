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
#include "SimulationAirController.h"
#if defined(AIR_ALL) || !defined(AIR_SIMULATION)
#include "HardwareAirControllers.h"
#endif

namespace melodica {

// Storage large enough (and correctly aligned) for the biggest air module.
// static_assert in createAirController() guards against a module outgrowing it.
struct alignas(16) AirControllerStorage {
    unsigned char bytes[320];
};

// Placement-new a module into the storage with a compile-time size check.
template <class T, class... Args>
inline IAirController* placeAir(AirControllerStorage& s, Args&&... args) {
    static_assert(sizeof(T) <= sizeof(AirControllerStorage),
                  "AirControllerStorage too small for this air module");
    return new (s.bytes) T(static_cast<Args&&>(args)...);
}

inline IAirController* createAirController(const BoardConfig& cfg,
                                           AirControllerStorage& storage) {
    const AirConfig& a = cfg.air;

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
