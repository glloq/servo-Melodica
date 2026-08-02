#include "SafetyManager.h"
#include "mocks.h"
#include "native_test.h"

using namespace melodica;

namespace {
MidiEvent noteOn(uint8_t note, uint8_t vel, uint32_t ts) {
    MidiEvent e;
    e.type = MidiEventType::NoteOn;
    e.channel = 0; e.data1 = note; e.data2 = vel; e.timestampUs = ts;
    return e;
}
BoardConfig makeConfig() {
    BoardConfig c;
    c.air.airPrechargeMs = 0;
    c.safety.disableOutputsOnIdle = false;  // isolate the fault paths under test
    return c;
}
}  // namespace

void run_safety_i2c_fault() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();
    SafetyManager safety(cfg.safety, inst, keys, air);

    inst.handleEvent(noteOn(65, 100, 0));
    CHECK(keys.isPressed(0));

    // A PCA board drops off the I2C bus -> critical fault, safe state enforced.
    keys.setHealthy(false);
    safety.update(1000, /*connected*/ true);
    CHECK(safety.inFault());
    CHECK_EQ(safety.fault() == SafetyFault::I2cFault, true);
    CHECK_EQ(keys.pressedCount(), 0);   // keys released
    CHECK(!keys.outputsEnabled());      // PWM outputs disabled
    CHECK(air.emergency());             // air hard-stopped

    // Recovery once the board answers again and link is up.
    keys.setHealthy(true);
    safety.update(2000, true);
    CHECK(!safety.inFault());
    CHECK(keys.outputsEnabled());
}

void run_safety_stuck_key() {
    BoardConfig cfg = makeConfig();
    cfg.safety.maxKeyHoldMs = 1000;  // 1s watchdog
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();
    SafetyManager safety(cfg.safety, inst, keys, air);

    inst.handleEvent(noteOn(65, 100, 0));
    inst.update(0);
    safety.update(500000, true);   // 0.5s: fine
    CHECK(!safety.inFault());
    safety.update(1500000, true);  // 1.5s: exceeds max hold
    CHECK_EQ(safety.fault() == SafetyFault::KeyStuck, true);
}

void run_safety_comm_timeout() {
    BoardConfig cfg = makeConfig();
    cfg.safety.commTimeoutMs = 1000;
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();
    SafetyManager safety(cfg.safety, inst, keys, air);

    inst.handleEvent(noteOn(65, 100, 0));
    safety.noteMidiActivity(0);

    // Link is down and no MIDI for > timeout while a note is active -> safe stop.
    safety.update(2000000, /*connected*/ false);
    CHECK(safety.inFault());
    CHECK_EQ(keys.pressedCount(), 0);
}
