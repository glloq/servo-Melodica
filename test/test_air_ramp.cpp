#include "AirRamp.h"
#include "native_test.h"

using namespace melodica;

void run_air_ramp() {
    AirConfig cfg;
    cfg.startThreshold = 8;
    cfg.minOutput = 20;
    cfg.maxOutput = 200;
    cfg.attackRampMs = 100;
    cfg.releaseRampMs = 100;
    cfg.precharge = 0;
    cfg.inverted = false;

    AirRamp ramp(cfg);

    // No sound => zero output.
    AirDemand d;
    d.soundRequested = false;
    ramp.setDemand(d);
    CHECK_EQ(ramp.update(0), 0);

    // Full demand ramps up over time, capped at maxOutput.
    d.soundRequested = true;
    d.expression = 255;
    ramp.setDemand(d);
    ramp.update(1000);          // establish dt baseline
    uint8_t l1 = ramp.update(51000);   // +50ms => ~half of full-scale step
    CHECK(l1 > 0);
    uint8_t l2 = ramp.update(201000);  // plenty of time => reach target
    CHECK(l2 >= l1);
    CHECK(l2 <= cfg.maxOutput);
    CHECK(l2 >= cfg.maxOutput - 5);  // effectively at ceiling

    // Demand below threshold closes.
    d.expression = 4;  // < startThreshold
    ramp.setDemand(d);
    for (int i = 0; i < 10; ++i) ramp.update(201000 + i * 50000);
    CHECK_EQ(ramp.rawLevel(), 0);

    // Emergency force-closed is immediate.
    d.expression = 255;
    ramp.setDemand(d);
    ramp.update(700000);
    CHECK_EQ(ramp.forceClosed(), 0);
    CHECK_EQ(ramp.rawLevel(), 0);

    // Inversion: closed actuator reads 255.
    AirConfig inv = cfg;
    inv.inverted = true;
    AirRamp rinv(inv);
    AirDemand off;
    off.soundRequested = false;
    rinv.setDemand(off);
    CHECK_EQ(rinv.update(0), 255);  // inverted closed
}
