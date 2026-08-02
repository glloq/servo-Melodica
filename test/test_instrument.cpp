#include "InstrumentController.h"
#include "mocks.h"
#include "native_test.h"

using namespace melodica;

namespace {

MidiEvent noteOn(uint8_t note, uint8_t vel, uint8_t ch, uint32_t ts) {
    MidiEvent e;
    e.type = MidiEventType::NoteOn;
    e.channel = ch; e.data1 = note; e.data2 = vel; e.timestampUs = ts;
    return e;
}
MidiEvent noteOff(uint8_t note, uint8_t ch, uint32_t ts) {
    MidiEvent e;
    e.type = MidiEventType::NoteOff;
    e.channel = ch; e.data1 = note; e.data2 = 0; e.timestampUs = ts;
    return e;
}
MidiEvent cc(uint8_t num, uint8_t val, uint8_t ch, uint32_t ts) {
    MidiEvent e;
    e.type = MidiEventType::ControlChange;
    e.channel = ch; e.data1 = num; e.data2 = val; e.timestampUs = ts;
    return e;
}

BoardConfig makeConfig() {
    BoardConfig c;
    c.air.policy = AirPolicy::MaximumVelocity;
    c.air.airPrechargeMs = 0;   // immediate press by default
    c.air.keyPressDelayMs = 0;
    c.air.keyReleaseDelayMs = 0;
    c.air.airReleaseDelayMs = 0;
    return c;
}

}  // namespace

void run_instrument_notes() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    // Note 65 -> key 0.
    inst.handleEvent(noteOn(65, 100, 0, 0));
    CHECK(keys.isPressed(0));
    CHECK(air.demand().soundRequested);

    inst.handleEvent(noteOff(65, 0, 100));
    CHECK(!keys.isPressed(0));
    CHECK(!air.demand().soundRequested);  // no notes => air closed

    // Highest key: note 96 -> key 31.
    inst.handleEvent(noteOn(96, 90, 0, 200));
    CHECK(keys.isPressed(31));

    // Out-of-range notes rejected (below and above).
    CHECK(!inst.handleEvent(noteOn(64, 100, 0, 300)));   // below FIRST_MIDI_NOTE
    CHECK(!inst.handleEvent(noteOn(97, 100, 0, 300)));   // above last key
}

void run_instrument_velocity_zero() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    inst.handleEvent(noteOn(70, 80, 0, 0));
    CHECK(keys.isPressed(5));
    // Note On with velocity 0 == Note Off.
    inst.handleEvent(noteOn(70, 0, 0, 100));
    CHECK(!keys.isPressed(5));
    CHECK(!air.demand().soundRequested);
}

void run_instrument_cc7_zero() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    inst.handleEvent(noteOn(72, 120, 0, 0));
    CHECK(air.demand().soundRequested);
    CHECK(air.demand().expression > 0);

    // CC7 = 0 must fully close the air even though the key is still down.
    inst.handleEvent(cc(7, 0, 0, 100));
    CHECK(!air.demand().soundRequested);
    CHECK_EQ(air.demand().expression, 0);
    CHECK(keys.isPressed(7));  // key stays pressed; only air closes

    // Restoring volume reopens the air.
    inst.handleEvent(cc(7, 127, 0, 200));
    CHECK(air.demand().soundRequested);
}

void run_instrument_chord_and_loudest_release() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    inst.handleEvent(noteOn(65, 40, 0, 0));
    inst.handleEvent(noteOn(69, 110, 0, 0));  // loudest
    inst.handleEvent(noteOn(72, 60, 0, 0));
    CHECK_EQ(inst.activeKeyCount(), 3);
    uint8_t withLoud = air.demand().expression;
    CHECK_EQ(air.demand().maxVelocity, 110);

    // Stop the loudest note: air must fall automatically.
    inst.handleEvent(noteOff(69, 0, 100));
    CHECK_EQ(inst.activeKeyCount(), 2);
    CHECK(air.demand().expression < withLoud);
    CHECK(air.demand().soundRequested);
}

void run_instrument_sustain() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    inst.handleEvent(cc(64, 127, 0, 0));    // sustain on
    inst.handleEvent(noteOn(74, 90, 0, 10));
    CHECK(keys.isPressed(9));

    // Note off while sustained: key stays down.
    inst.handleEvent(noteOff(74, 0, 20));
    CHECK(keys.isPressed(9));
    CHECK(inst.note(9).sustained);

    // Release pedal: key now lifts.
    inst.handleEvent(cc(64, 0, 0, 30));
    CHECK(!keys.isPressed(9));
    CHECK(!air.demand().soundRequested);
}

void run_instrument_all_notes_off() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    inst.handleEvent(noteOn(65, 80, 0, 0));
    inst.handleEvent(noteOn(70, 80, 1, 0));  // different channel
    CHECK_EQ(inst.activeKeyCount(), 2);

    inst.handleEvent(cc(123, 0, 0, 100));  // All Notes Off
    CHECK_EQ(keys.pressedCount(), 0);
    CHECK_EQ(inst.activeKeyCount(), 0);
    CHECK(air.stopped());

    // All Sound Off triggers emergency air stop.
    inst.handleEvent(noteOn(65, 80, 0, 200));
    inst.handleEvent(cc(120, 0, 0, 300));
    CHECK_EQ(keys.pressedCount(), 0);
    CHECK(air.emergency());
}

void run_instrument_multichannel() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    // Same note from two channels: reference-counted, releases on last off.
    inst.handleEvent(noteOn(65, 80, 0, 0));
    inst.handleEvent(noteOn(65, 90, 1, 0));
    CHECK(keys.isPressed(0));
    inst.handleEvent(noteOff(65, 0, 100));  // channel 0 releases
    CHECK(keys.isPressed(0));                // still held by channel 1
    inst.handleEvent(noteOff(65, 1, 200));  // channel 1 releases
    CHECK(!keys.isPressed(0));
}

void run_instrument_precharge_nonblocking() {
    BoardConfig cfg = makeConfig();
    cfg.air.airPrechargeMs = 30;  // 30ms precharge before pressing
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    uint32_t t = 1000000;  // 1s in us
    inst.handleEvent(noteOn(65, 100, 0, t));
    // Air opens immediately, key NOT yet pressed.
    CHECK(air.demand().soundRequested);
    CHECK(!keys.isPressed(0));

    // Before the precharge elapses the key is still up (non-blocking wait).
    inst.update(t + 10000);  // +10ms
    CHECK(!keys.isPressed(0));

    // After the precharge window the scheduler presses the key.
    inst.update(t + 31000);  // +31ms
    CHECK(keys.isPressed(0));
}

void run_instrument_geometry() {
    // A 25-key melodica starting at MIDI 60 (C4), configured at runtime.
    BoardConfig cfg = makeConfig();
    cfg.instrument.noteCount = 25;
    cfg.instrument.firstMidiNote = 60;
    MockKeyDriver keys(MAX_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();
    CHECK_EQ(inst.keyCount(), 25);

    inst.handleEvent(noteOn(60, 100, 0, 0));   // first key
    CHECK(keys.isPressed(0));
    inst.handleEvent(noteOn(84, 100, 0, 0));   // 60+24 = last key
    CHECK(keys.isPressed(24));
    CHECK(!inst.handleEvent(noteOn(85, 100, 0, 0)));  // one past the top
    CHECK(!inst.handleEvent(noteOn(59, 100, 0, 0)));  // below the bottom
}

void run_instrument_transport_loss() {
    BoardConfig cfg = makeConfig();
    MockKeyDriver keys(NUMBER_OF_NOTES);
    MockAirController air;
    InstrumentController inst(cfg, keys, air);
    inst.begin();

    inst.handleEvent(noteOn(65, 80, 0, 0));
    inst.handleEvent(noteOn(70, 80, 0, 0));
    CHECK_EQ(inst.activeKeyCount(), 2);

    // Losing BLE/WiFi triggers a safe stop.
    inst.onTransportLost(100);
    CHECK_EQ(keys.pressedCount(), 0);
    CHECK_EQ(inst.activeKeyCount(), 0);
    CHECK(air.stopped());
}
