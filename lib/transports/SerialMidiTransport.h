#ifndef TRANSPORTS_SERIALMIDITRANSPORT_H
#define TRANSPORTS_SERIALMIDITRANSPORT_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "EventQueue.h"
#include "IMidiTransport.h"
#include "MidiParser.h"

namespace melodica {

// Raw MIDI bytes over a dedicated hardware UART, typically from a Raspberry Pi
// host running a MIDI-to-serial bridge. It owns its own UART (default Serial2 at
// 115200) so it never shares a port with the log/console on Serial — mixing MIDI
// bytes and log text on one UART would corrupt both.
class SerialMidiTransport : public IMidiTransport {
public:
    explicit SerialMidiTransport(HardwareSerial& uart, uint32_t baud = 115200,
                                 int8_t rxPin = -1, int8_t txPin = -1)
        : uart_(uart), baud_(baud), rxPin_(rxPin), txPin_(txPin) {}

    bool begin() override {
        uart_.begin(baud_, SERIAL_8N1, rxPin_, txPin_);
        return true;
    }

    void update() override {
        MidiEvent e;
        while (uart_.available() > 0) {
            uint8_t b = static_cast<uint8_t>(uart_.read());
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
    HardwareSerial& uart_;
    uint32_t baud_;
    int8_t rxPin_;
    int8_t txPin_;
    MidiParser parser_;
    EventQueue<64> queue_;
    uint32_t lastByteMs_ = 0;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // TRANSPORTS_SERIALMIDITRANSPORT_H
