#include "EventQueue.h"
#include "MidiParser.h"
#include "native_test.h"

using namespace melodica;

void run_midi_parser() {
    MidiParser p;
    MidiEvent e;

    // Note On, channel 2, note 60, velocity 100.
    CHECK(!p.feed(0x92, 0, e));
    CHECK(!p.feed(60, 0, e));
    CHECK(p.feed(100, 0, e));
    CHECK_EQ(e.type == MidiEventType::NoteOn, true);
    CHECK_EQ(e.channel, 2);
    CHECK_EQ(e.data1, 60);
    CHECK_EQ(e.data2, 100);

    // Running status: another note without repeating the status byte.
    CHECK(!p.feed(62, 0, e));
    CHECK(p.feed(90, 0, e));
    CHECK_EQ(e.type == MidiEventType::NoteOn, true);
    CHECK_EQ(e.data1, 62);

    // Control change.
    CHECK(!p.feed(0xB0, 0, e));
    CHECK(!p.feed(7, 0, e));
    CHECK(p.feed(0, 0, e));
    CHECK_EQ(e.type == MidiEventType::ControlChange, true);
    CHECK_EQ(e.data1, 7);
    CHECK_EQ(e.data2, 0);

    // Pitch bend centre.
    CHECK(!p.feed(0xE0, 0, e));
    CHECK(!p.feed(0x00, 0, e));
    CHECK(p.feed(0x40, 0, e));
    CHECK_EQ(e.type == MidiEventType::PitchBend, true);
    CHECK_EQ(e.signedValue, 0);

    // Real-time Clock interleaved is single-byte.
    CHECK(p.feed(0xF8, 0, e));
    CHECK_EQ(e.type == MidiEventType::Clock, true);

    // Program change is one data byte.
    CHECK(!p.feed(0xC5, 0, e));
    CHECK(p.feed(12, 0, e));
    CHECK_EQ(e.type == MidiEventType::ProgramChange, true);
    CHECK_EQ(e.channel, 5);
    CHECK_EQ(e.data1, 12);
}

void run_event_queue() {
    EventQueue<4> q;
    MidiEvent e;
    CHECK(q.empty());
    CHECK(!q.pop(e));

    MidiEvent a; a.data1 = 1;
    MidiEvent b; b.data1 = 2;
    q.push(a);
    q.push(b);
    CHECK(!q.empty());
    CHECK(q.pop(e)); CHECK_EQ(e.data1, 1);
    CHECK(q.pop(e)); CHECK_EQ(e.data1, 2);
    CHECK(q.empty());

    // Overflow drops the oldest (capacity 4 => 3 usable slots).
    for (uint8_t i = 0; i < 10; ++i) { MidiEvent x; x.data1 = i; q.push(x); }
    CHECK(q.drops() > 0);
    CHECK(q.pop(e));  // still yields the most recent events
}
