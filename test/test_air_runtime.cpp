#include "AirRamp.h"
#include "AirRunTracker.h"
#include "SafetyManager.h"
#include "mocks.h"
#include "native_test.h"

using namespace melodica;

namespace {
AirDemand demand(bool sound, uint8_t expr) {
    AirDemand d;
    d.soundRequested = sound;
    d.expression = expr;
    d.maxVelocity = expr;
    d.activeNoteCount = sound ? 1 : 0;
    return d;
}
BoardConfig makeConfig() {
    BoardConfig c;
    c.air.airPrechargeMs = 0;
    c.safety.disableOutputsOnIdle = false;
    return c;
}
}  // namespace

void run_air_run_tracker() {
    AirRunTracker t;
    CHECK(!t.running());
    CHECK_EQ(t.runtimeMs(), 0);

    t.update(1000000u, true);
    CHECK(t.running());
    CHECK_EQ(t.runtimeMs(), 0);

    t.update(1500000u, true);
    CHECK_EQ(t.runtimeMs(), 500);
    t.update(4000000u, true);
    CHECK_EQ(t.runtimeMs(), 3000);

    // Stopping clears the accumulator; restarting counts from zero again.
    t.update(4500000u, false);
    CHECK(!t.running());
    CHECK_EQ(t.runtimeMs(), 0);
    t.update(5000000u, true);
    t.update(5250000u, true);
    CHECK_EQ(t.runtimeMs(), 250);

    // Wrap-safe across the micros() rollover.
    AirRunTracker w;
    w.update(0xFFFFF000u, true);
    w.update(0x00000FA0u, true);  // 8096 us later, having wrapped
    CHECK(w.running());
    CHECK_EQ(w.runtimeMs(), 8);
}

void run_air_ramp_runtime() {
    AirConfig cfg;
    cfg.attackRampMs = 0;
    cfg.releaseRampMs = 0;
    cfg.minOutput = 20;
    cfg.maxOutput = 200;
    AirRamp ramp(cfg);

    ramp.setDemand(demand(true, 127));
    ramp.update(1000000u);
    CHECK(ramp.isRunning());
    ramp.update(3000000u);
    CHECK_EQ(ramp.runtimeMs(), 2000);

    ramp.setDemand(demand(false, 0));
    ramp.update(3100000u);
    CHECK(!ramp.isRunning());
    CHECK_EQ(ramp.runtimeMs(), 0);

    // An inverted actuator is still "not running" when no air flows: the tracker
    // follows the pre-inversion level.
    AirConfig inv = cfg;
    inv.inverted = true;
    AirRamp iramp(inv);
    iramp.setDemand(demand(false, 0));
    CHECK_EQ(iramp.update(1000u), 255);  // output is high...
    CHECK(!iramp.isRunning());           // ...but the valve is closed

    // forceClosed() resets the accumulator.
    AirRamp r2(cfg);
    r2.setDemand(demand(true, 127));
    r2.update(1000000u);
    r2.update(9000000u);
    CHECK(r2.runtimeMs() > 0);
    r2.forceClosed();
    CHECK(!r2.isRunning());
    CHECK_EQ(r2.runtimeMs(), 0);
}

// SafetyConfig::maxPumpRunMs used to be documented and never checked: a pump
// could run for ever on a dead sensor. It is now enforced.
void run_safety_pump_overrun() {
    BoardConfig cfg = makeConfig();
    cfg.safety.maxPumpRunMs = 2000;
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();
    SafetyManager safety(cfg.safety, inst, keys, air);

    air.setRunning(true, 1500);
    safety.update(1000, true);
    CHECK(!safety.inFault());  // still inside the allowance

    air.setRunning(true, 2001);
    safety.update(2000, true);
    CHECK_EQ(safety.fault() == SafetyFault::PumpOverrun, true);
    CHECK(air.emergency());        // air hard-stopped
    CHECK(!keys.outputsEnabled());

    // Latched: it does not clear itself even once the pump is idle again.
    air.setRunning(false, 0);
    safety.update(3000, true);
    CHECK_EQ(safety.fault() == SafetyFault::PumpOverrun, true);
    safety.clearFault();
    CHECK(!safety.inFault());

    // A zero limit means "watchdog disabled".
    BoardConfig off = makeConfig();
    off.safety.maxPumpRunMs = 0;
    MockKeyDriver k2(NUMBER_OF_NOTES);
    MockAirController a2;
    InstrumentController i2(off, k2, a2);
    i2.begin();
    SafetyManager s2(off.safety, i2, k2, a2);
    a2.setRunning(true, 3600000u);
    s2.update(1000, true);
    CHECK(!s2.inFault());
}

// An air module that refuses to run (bad pin, reservoir without sensor) must
// latch a fault rather than being commanded anyway.
void run_safety_air_unhealthy() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();
    SafetyManager safety(cfg.safety, inst, keys, air);

    safety.update(1000, true);
    CHECK(!safety.inFault());

    air.setFault(AirFault::InvalidSensor);
    safety.update(2000, true);
    CHECK_EQ(safety.fault() == SafetyFault::AirNotInitialised, true);
    CHECK(!keys.outputsEnabled());

    // Recovers only when the module reports itself healthy again.
    safety.update(3000, true);
    CHECK(safety.inFault());
    air.setFault(AirFault::None);
    safety.update(4000, true);
    CHECK(!safety.inFault());
}

// The first cause of a shutdown is the one worth reporting: a second symptom
// must not overwrite it (and must not re-run the safe stop on every loop).
void run_safety_first_fault_wins() {
    BoardConfig cfg = makeConfig();
    cfg.safety.maxPumpRunMs = 1000;
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();
    SafetyManager safety(cfg.safety, inst, keys, air);

    keys.setHealthy(false);
    safety.update(1000, true);
    CHECK_EQ(safety.fault() == SafetyFault::I2cFault, true);

    air.setRunning(true, 5000);
    safety.update(2000, true);
    CHECK_EQ(safety.fault() == SafetyFault::I2cFault, true);  // still the first cause
}
