#include "InstrumentController.h"
#include "SimulationAirController.h"
#include "mocks.h"
#include "native_test.h"

using namespace melodica;

namespace {
MidiEvent noteOn(uint8_t note, uint8_t vel, uint32_t ts) {
    MidiEvent e; e.type = MidiEventType::NoteOn; e.channel = 0;
    e.data1 = note; e.data2 = vel; e.timestampUs = ts; return e;
}
MidiEvent cc(uint8_t num, uint8_t val, uint32_t ts) {
    MidiEvent e; e.type = MidiEventType::ControlChange; e.channel = 0;
    e.data1 = num; e.data2 = val; e.timestampUs = ts; return e;
}
}  // namespace

// Drive the core through a real SimulationAirController (same code path as the
// hardware modules, minus the actuator write) to prove the whole air pipeline.
void run_air_module_end_to_end() {
    BoardConfig cfg;
    cfg.air.airPrechargeMs = 0;
    cfg.air.airReleaseDelayMs = 0;
    cfg.air.attackRampMs = 0;   // instant for deterministic assertions
    cfg.air.releaseRampMs = 0;
    cfg.air.startThreshold = 4;
    cfg.air.minOutput = 20;
    cfg.air.maxOutput = 200;

    MockKeyDriver keys(NUMBER_OF_NOTES);
    SimulationAirController air(cfg.air);
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    uint32_t t = 1000;
    inst.handleEvent(noteOn(65, 127, t));
    inst.update(t += 1000);
    CHECK(air.level() > 0);  // air flowing for a loud note

    // CC7 = 0 must drive the actuator level to zero.
    inst.handleEvent(cc(7, 0, t));
    inst.update(t += 1000);
    CHECK_EQ(air.level(), 0);

    // Restore volume -> air returns.
    inst.handleEvent(cc(7, 127, t));
    inst.update(t += 1000);
    CHECK(air.level() > 0);

    // Emergency stop forces zero immediately.
    air.emergencyStop();
    CHECK_EQ(air.level(), 0);
}
