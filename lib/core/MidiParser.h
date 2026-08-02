#ifndef CORE_MIDIPARSER_H
#define CORE_MIDIPARSER_H

#include <cstdint>

#include "MidiEvent.h"

namespace melodica {

// Byte-at-a-time parser for a raw MIDI stream (DIN/UART or serial-from-Pi).
// Handles channel voice messages, running status and System Real-Time bytes.
// Pure and host-testable; the DIN/serial transports feed it bytes and forward
// the completed MidiEvent. SysEx is skipped (not needed by this instrument).
class MidiParser {
public:
    // Feed one byte. If it completes a message, fills `out` and returns true.
    // `nowUs` stamps the produced event.
    bool feed(uint8_t byte, uint32_t nowUs, MidiEvent& out) {
        if (byte >= 0xF8) {
            // System Real-Time: single byte, may interleave anywhere.
            return realtime(byte, nowUs, out);
        }
        if (byte & 0x80) {
            // Status byte.
            if (byte >= 0xF0) {
                // System Common: 0xF0 starts SysEx (skip until 0xF7).
                if (byte == 0xF0) { inSysex_ = true; return false; }
                if (byte == 0xF7) { inSysex_ = false; return false; }
                status_ = 0;  // other system-common messages reset running status
                return false;
            }
            status_ = byte;
            expected_ = dataBytesFor(byte);
            haveData_ = 0;
            return false;
        }
        // Data byte.
        if (inSysex_ || status_ == 0) return false;  // no context
        data_[haveData_++] = byte;
        if (haveData_ >= expected_) {
            haveData_ = 0;  // running status: keep status_ for next message
            return build(nowUs, out);
        }
        return false;
    }

private:
    static uint8_t dataBytesFor(uint8_t status) {
        uint8_t hi = status & 0xF0;
        if (hi == 0xC0 || hi == 0xD0) return 1;  // program change / channel pressure
        return 2;
    }

    bool realtime(uint8_t byte, uint32_t nowUs, MidiEvent& out) {
        out = MidiEvent{};
        out.timestampUs = nowUs;
        switch (byte) {
            case 0xF8: out.type = MidiEventType::Clock; return true;
            case 0xFA: out.type = MidiEventType::Start; return true;
            case 0xFB: out.type = MidiEventType::Continue; return true;
            case 0xFC: out.type = MidiEventType::Stop; return true;
            case 0xFF: out.type = MidiEventType::SystemReset; return true;
            default: return false;  // 0xF9/0xFD undefined, 0xFE active sensing
        }
    }

    bool build(uint32_t nowUs, MidiEvent& out) {
        out = MidiEvent{};
        out.channel = status_ & 0x0F;
        out.timestampUs = nowUs;
        switch (status_ & 0xF0) {
            case 0x80:
                out.type = MidiEventType::NoteOff;
                out.data1 = data_[0]; out.data2 = data_[1];
                return true;
            case 0x90:
                out.type = MidiEventType::NoteOn;
                out.data1 = data_[0]; out.data2 = data_[1];
                return true;
            case 0xA0:
                out.type = MidiEventType::PolyPressure;
                out.data1 = data_[0]; out.data2 = data_[1];
                return true;
            case 0xB0:
                out.type = MidiEventType::ControlChange;
                out.data1 = data_[0]; out.data2 = data_[1];
                return true;
            case 0xC0:
                out.type = MidiEventType::ProgramChange;
                out.data1 = data_[0];
                return true;
            case 0xD0:
                out.type = MidiEventType::ChannelPressure;
                out.data1 = data_[0];
                return true;
            case 0xE0:
                out.type = MidiEventType::PitchBend;
                out.data1 = data_[0]; out.data2 = data_[1];
                out.signedValue = static_cast<int16_t>(((data_[1] << 7) | data_[0]) - 8192);
                return true;
            default:
                return false;
        }
    }

    uint8_t status_ = 0;
    uint8_t expected_ = 0;
    uint8_t haveData_ = 0;
    uint8_t data_[2] = {0, 0};
    bool inSysex_ = false;
};

}  // namespace melodica

#endif  // CORE_MIDIPARSER_H
