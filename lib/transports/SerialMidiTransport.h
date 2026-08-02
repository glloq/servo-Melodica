#ifndef TRANSPORTS_SERIALMIDITRANSPORT_H
#define TRANSPORTS_SERIALMIDITRANSPORT_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "EventQueue.h"
#include "IMidiTransport.h"
#include "MidiParser.h"

namespace melodica {

// Raw MIDI bytes over a serial link, typically from a Raspberry Pi host running
// a MIDI-to-serial bridge. Identical parsing to DIN but on an arbitrary baud
// rate (default 115200) and stream.
class SerialMidiTransport : public IMidiTransport {
public:
    explicit SerialMidiTransport(Stream& stream) : stream_(stream) {}

    bool begin() override { return true; }  // caller owns Serial.begin()

    void update() override {
        MidiEvent e;
        while (stream_.available() > 0) {
            uint8_t b = static_cast<uint8_t>(stream_.read());
            lastByteMs_ = millis();
            if (parser_.feed(b, micros(), e)) queue_.push(e);
        }
    }

    bool read(MidiEvent& event) override { return queue_.pop(event); }

    bool connected() const override {
        return lastByteMs_ != 0 && (millis() - lastByteMs_) < 3000;
    }

    const char* name() const override { return "Serial-MIDI"; }

private:
    Stream& stream_;
    MidiParser parser_;
    EventQueue<64> queue_;
    uint32_t lastByteMs_ = 0;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // TRANSPORTS_SERIALMIDITRANSPORT_H
