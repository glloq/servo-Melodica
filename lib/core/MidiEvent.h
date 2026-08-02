#ifndef CORE_MIDIEVENT_H
#define CORE_MIDIEVENT_H

#include <cstdint>

namespace melodica {

// Transport-agnostic MIDI event. Every IMidiTransport adapter (BLE, RTP, DIN,
// USB, serial) normalises its wire format into this single structure so the
// instrument core never depends on a concrete MIDI library.
enum class MidiEventType : uint8_t {
    NoteOn,
    NoteOff,
    ControlChange,
    PitchBend,
    ProgramChange,
    ChannelPressure,
    PolyPressure,
    Clock,
    Start,
    Stop,
    Continue,
    SystemReset
};

struct MidiEvent {
    MidiEventType type = MidiEventType::SystemReset;
    uint8_t channel = 0;       // 0..15
    uint8_t data1 = 0;         // note / controller / program
    uint8_t data2 = 0;         // velocity / value
    int16_t signedValue = 0;   // pitch bend (-8192..+8191), else 0
    uint32_t timestampUs = 0;  // capture time in microseconds
};

// Well-known Control Change numbers handled by the core. Using an enum class
// instead of scattered magic numbers keeps the switch statements self-documenting.
enum class ControlChange : uint8_t {
    Volume = 7,
    Expression = 11,
    Sustain = 64,
    AllSoundOff = 120,
    ResetAllControllers = 121,
    AllNotesOff = 123
};

}  // namespace melodica

#endif  // CORE_MIDIEVENT_H
