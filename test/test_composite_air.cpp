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
    bool started = false;
    uint32_t runtime = 0;
    bool begin() override { started = true; return true; }
    void request(bool a, uint8_t d) override { active = a; demand = d; }
    void update(uint32_t, uint8_t p) override { lastPressure = p; }
    void stop() override { stopped = true; active = false; }
    bool isRunning() const override { return runtime > 0; }
    uint32_t runtimeMs() const override { return runtime; }
};
struct MockSensor : IPressureSensor {
    uint8_t val = 128;
    bool ok = true;
    bool begin() override { return ok; }
    uint8_t readNormalized() override { return val; }
    bool valid() const override { return ok; }
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
    // The source follows the SHAPED level, not the raw MIDI expression, so the
    // attack/release envelope reaches a blower that is the only proportional
    // element in the chain. expression 127 maps into [minOutput, maxOutput].
    CHECK_EQ(source.demand, flow.level);
    CHECK_EQ(source.demand, 20 + (180 * 127) / 255);
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

// A regulated reservoir with no usable pressure sensor reads "0 bar" for ever
// and would keep its pump running until something melts. The controller refuses
// to start at all, and reports why.
void run_composite_reservoir_requires_sensor() {
    AirConfig cfg;
    cfg.composite.source = SourceKind::Reservoir;

    MockValve valve; MockFlow flow; MockSource source;
    CompositeAirController noSensor(cfg, valve, flow, source, nullptr);
    CHECK(!noSensor.begin());
    CHECK(!noSensor.healthy());
    CHECK_EQ(noSensor.fault() == AirFault::InvalidSensor, true);
    CHECK(!source.started);
    CHECK(source.stopped);

    // A faulted controller never drives its stages, whatever the demand.
    noSensor.setDemand(demand(true, 127));
    noSensor.update(1000);
    CHECK(!valve.open);
    CHECK_EQ(flow.level, 0);
    CHECK(!source.active);

    // A sensor that cannot produce a reading (bad pin / degenerate range) is no
    // better than no sensor at all.
    MockSensor broken;
    broken.ok = false;
    MockValve v2; MockFlow f2; MockSource s2;
    CompositeAirController badSensor(cfg, v2, f2, s2, &broken);
    CHECK(!badSensor.begin());
    CHECK_EQ(badSensor.fault() == AirFault::InvalidSensor, true);

    // With a valid sensor it starts normally.
    MockSensor good;
    MockValve v3; MockFlow f3; MockSource s3;
    CompositeAirController ok(cfg, v3, f3, s3, &good);
    CHECK(ok.begin());
    CHECK(ok.healthy());
    CHECK(s3.started);
}

// The overrun watchdog has to see a reservoir pump that runs with no notes
// playing, so the composite reports the longest run of any stage.
void run_composite_runtime_reporting() {
    AirConfig cfg;
    cfg.attackRampMs = 0;
    cfg.releaseRampMs = 0;
    MockValve valve; MockFlow flow; MockSource source; MockSensor sensor;
    CompositeAirController comp(cfg, valve, flow, source, &sensor);
    CHECK(comp.begin());

    CHECK(!comp.isRunning());
    CHECK_EQ(comp.runtimeMs(), 0);

    // Source running on its own (reservoir filling), no note played.
    source.runtime = 45000;
    CHECK(comp.isRunning());
    CHECK_EQ(comp.runtimeMs(), 45000);

    // A shorter note-driven run does not shorten the reported duty.
    comp.setDemand(demand(true, 200));
    comp.update(1000000u);
    comp.update(3000000u);
    CHECK_EQ(comp.runtimeMs(), 45000);
    source.runtime = 0;
    CHECK_EQ(comp.runtimeMs(), 2000);
}
