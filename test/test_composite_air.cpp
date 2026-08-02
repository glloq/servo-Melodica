#include "AirStages.h"
#include "native_test.h"

using namespace melodica;

namespace {
struct MockValve : IMainValve {
    bool open = false;
    bool begin() override { return true; }
    void set(bool o) override { open = o; }
    void update(uint32_t) override {}
};
struct MockFlow : IFlowActuator {
    uint8_t level = 0;
    bool begin() override { return true; }
    void setLevel(uint8_t l) override { level = l; }
    void update(uint32_t) override {}
};
struct MockSource : IAirSource {
    bool active = false;
    uint8_t demand = 0;
    uint8_t lastPressure = 0;
    bool stopped = false;
    bool begin() override { return true; }
    void request(bool a, uint8_t d) override { active = a; demand = d; }
    void update(uint32_t, uint8_t p) override { lastPressure = p; }
    void stop() override { stopped = true; active = false; }
};
struct MockSensor : IPressureSensor {
    uint8_t val = 128;
    bool begin() override { return true; }
    uint8_t readNormalized() override { return val; }
};

AirDemand demand(bool sound, uint8_t expr) {
    AirDemand d;
    d.soundRequested = sound;
    d.expression = expr;
    d.maxVelocity = expr;
    d.activeNoteCount = sound ? 1 : 0;
    return d;
}
}  // namespace

void run_reservoir_regulator() {
    // Below the hysteresis band => start the pump.
    CHECK(ReservoirRegulator::pumpShouldRun(false, 170, 200, 20));   // 170 < 180
    // Within the band while off => stay off.
    CHECK(!ReservoirRegulator::pumpShouldRun(false, 190, 200, 20));  // 190 >= 180
    // While filling, keep going until the target is reached.
    CHECK(ReservoirRegulator::pumpShouldRun(true, 199, 200, 20));
    CHECK(!ReservoirRegulator::pumpShouldRun(true, 200, 200, 20));   // reached target
    // target smaller than hysteresis clamps the low bound at 0.
    CHECK(!ReservoirRegulator::pumpShouldRun(false, 5, 10, 20));
}

void run_composite_air() {
    AirConfig cfg;
    cfg.startThreshold = 8;
    cfg.minOutput = 20;
    cfg.maxOutput = 200;
    cfg.attackRampMs = 0;
    cfg.releaseRampMs = 0;

    MockValve valve;
    MockFlow flow;
    MockSource source;
    MockSensor sensor;
    CompositeAirController comp(cfg, valve, flow, source, &sensor);
    comp.begin();

    // Sound requested: main valve opens, flow modulates, source runs, and the
    // sensor reading is forwarded to the source (for reservoir regulation).
    comp.setDemand(demand(true, 127));
    comp.update(1000);
    CHECK(valve.open);
    CHECK(flow.level > 0);
    CHECK(source.active);
    CHECK_EQ(source.demand, 127);
    CHECK_EQ(source.lastPressure, 128);  // from the sensor

    // Silence: valve closes, flow falls to zero, source idles.
    comp.setDemand(demand(false, 0));
    comp.update(2000);
    CHECK(!valve.open);
    CHECK_EQ(flow.level, 0);
    CHECK(!source.active);

    // Emergency stop hard-closes the valve and stops the source.
    comp.setDemand(demand(true, 127));
    comp.update(3000);
    comp.emergencyStop();
    CHECK(!valve.open);
    CHECK(source.stopped);

    // Without a sensor, the source receives full pressure (open loop).
    MockValve v2; MockFlow f2; MockSource s2;
    CompositeAirController comp2(cfg, v2, f2, s2, nullptr);
    comp2.begin();
    comp2.setDemand(demand(true, 100));
    comp2.update(1000);
    CHECK_EQ(s2.lastPressure, 255);
}
